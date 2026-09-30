"""Simple standard-tuning guitar analysis and session tracking."""

from __future__ import annotations

import math
import queue
import sys
import time
from dataclasses import dataclass, field
from typing import Callable

import numpy as np
import sounddevice as sd
from scipy.fft import rfft, rfftfreq
from scipy.signal import find_peaks, get_window

from light_renderer import NullLightRenderer, SevenLightGauge


@dataclass(frozen=True)
class StringTarget:
    name: str
    frequency: float


STANDARD_TUNING = (
    StringTarget("E2", 82.4069),
    StringTarget("A2", 110.0000),
    StringTarget("D3", 146.8324),
    StringTarget("G3", 195.9977),
    StringTarget("B3", 246.9417),
    StringTarget("E4", 329.6276),
)

SAMPLE_RATE = 48_000
HOP_SIZE = 2_048
WINDOW_SIZE = 8_192
FFT_SIZE = 65_536
LIGHT_OFF_DELAY_SECONDS = 5.0


@dataclass(frozen=True)
class PitchReading:
    frequency: float
    target: StringTarget
    cents: float
    harmonic_count: int
    signal_rms: float

    @property
    def direction(self) -> str:
        if abs(self.cents) <= 5.0:
            return "in tune"
        return "sharp" if self.cents > 0 else "flat"


@dataclass(frozen=True)
class SpectrumAnalysis:
    frequencies: np.ndarray
    magnitudes: np.ndarray
    peak_frequencies: np.ndarray
    peak_magnitudes: np.ndarray
    reading: PitchReading | None
    signal_rms: float
    required_rms: float


@dataclass
class DynamicNoiseFloor:
    value: float = 0.004

    def observe(self, signal_rms: float) -> None:
        alpha = 0.30 if signal_rms > self.value else 0.15
        capped_sample = min(signal_rms, self.value * 1.75)
        self.value = max(0.0005, self.value + alpha * (capped_sample - self.value))


def cents_between(frequency: float, target: float) -> float:
    return 1200.0 * math.log2(frequency / target)


def analyze_block(
    samples: np.ndarray,
    noise_rms: float,
    sample_rate: int = SAMPLE_RATE,
    minimum_snr_db: float = 6.0,
) -> SpectrumAnalysis:
    """Analyze one rolling window against standard guitar tuning."""
    signal = np.asarray(samples, dtype=np.float64).reshape(-1)
    signal -= np.mean(signal)
    signal_rms = float(np.sqrt(np.mean(signal * signal)))
    required_rms = max(0.0025, noise_rms * math.pow(10.0, minimum_snr_db / 20.0))

    window = get_window("hann", signal.size)
    magnitudes = 2.0 * np.abs(rfft(signal * window, n=FFT_SIZE)) / np.sum(window)
    frequencies = rfftfreq(FFT_SIZE, 1.0 / sample_rate)
    display = (frequencies >= 60.0) & (frequencies <= 1200.0)
    frequencies = frequencies[display]
    magnitudes = magnitudes[display]

    prominence = max(float(np.max(magnitudes)) * 0.04, noise_rms * 0.12)
    peaks, properties = find_peaks(
        magnitudes,
        prominence=prominence,
        distance=max(1, round(3.0 / (sample_rate / FFT_SIZE))),
    )
    peak_frequencies = frequencies[peaks].copy()
    peak_magnitudes = magnitudes[peaks].copy()
    # Zero-padding gives a smooth curve, and parabolic interpolation between
    # FFT bins avoids a several-cent quantization error on E2 and A2.
    bin_width = sample_rate / FFT_SIZE
    for output_index, peak_index in enumerate(peaks):
        if peak_index == 0 or peak_index == magnitudes.size - 1:
            continue
        left, center, right = np.log(
            np.maximum(magnitudes[peak_index - 1 : peak_index + 2], 1e-15)
        )
        denominator = left - 2.0 * center + right
        if denominator == 0.0:
            continue
        offset = float(np.clip(0.5 * (left - right) / denominator, -0.5, 0.5))
        peak_frequencies[output_index] += offset * bin_width

    reading = None
    if signal_rms >= required_rms and peaks.size:
        candidates: list[tuple[float, PitchReading]] = []
        peak_prominences = properties["prominences"]
        for target in STANDARD_TUNING:
            estimates: list[tuple[float, float]] = []
            score = 0.0
            for harmonic in range(1, 7):
                expected = target.frequency * harmonic
                if expected > 1200.0:
                    break
                errors = np.abs(1200.0 * np.log2(peak_frequencies / expected))
                matches = np.flatnonzero(errors <= 150.0)
                if not matches.size:
                    continue
                best = int(matches[np.argmax(peak_prominences[matches])])
                weight = float(peak_prominences[best]) / harmonic
                estimates.append((float(peak_frequencies[best]) / harmonic, weight))
                score += weight

            if len(estimates) < 2:
                continue
            estimated = sum(value * weight for value, weight in estimates) / sum(
                weight for _, weight in estimates
            )
            estimate_cents = np.array(
                [cents_between(value, estimated) for value, _ in estimates]
            )
            if float(np.std(estimate_cents)) > 25.0:
                continue
            cents = cents_between(estimated, target.frequency)
            if abs(cents) <= 150.0:
                candidates.append(
                    (
                        score,
                        PitchReading(
                            estimated,
                            target,
                            cents,
                            len(estimates),
                            signal_rms,
                        ),
                    )
                )
        if candidates:
            best_score = max(score for score, _reading in candidates)
            # A laptop microphone may almost remove an E2 fundamental. In
            # that case its 2nd/4th/6th harmonics also resemble E4. Prefer the
            # lowest well-supported candidate when its score remains close to
            # the strongest one, while a real E4 stays much stronger as E4.
            viable = [
                item for item in candidates if item[0] >= best_score * 0.65
            ]
            candidate_by_name = {
                candidate.target.name: candidate for _score, candidate in candidates
            }
            low_e = candidate_by_name.get("E2")
            high_e = candidate_by_name.get("E4")
            low_e_evidence = False
            if low_e is not None and high_e is not None:
                strong_peak = peak_prominences >= float(np.max(peak_prominences)) * 0.10
                for partial in (1, 2):
                    expected = STANDARD_TUNING[0].frequency * partial
                    errors = np.abs(
                        1200.0 * np.log2(peak_frequencies[strong_peak] / expected)
                    )
                    if np.any(errors <= 60.0):
                        low_e_evidence = True
                        break

            if low_e_evidence:
                # E2's fourth harmonic is almost exactly E4. A strong 82 Hz
                # or 165 Hz partial proves that this is the low string and
                # prevents one pluck from completing both E strings.
                reading = low_e
            else:
                reading = min(
                    viable, key=lambda item: item[1].target.frequency
                )[1]

    return SpectrumAnalysis(
        frequencies,
        magnitudes,
        peak_frequencies,
        peak_magnitudes,
        reading,
        signal_rms,
        required_rms,
    )


@dataclass
class TuningSession:
    """Track standard strings with tune and detune hysteresis."""

    epsilon_cents: float = 5.0
    confirmation_frames: int = 3
    detune_cents: float = 15.0
    detune_frames: int = 3
    tuned: set[str] = field(default_factory=set)
    _last_name: str | None = field(default=None, init=False)
    _tune_count: int = field(default=0, init=False)
    _detune_counts: dict[str, int] = field(default_factory=dict, init=False)

    @property
    def targets(self) -> tuple[StringTarget, ...]:
        return STANDARD_TUNING

    def update(self, reading: PitchReading) -> str | None:
        name = reading.target.name
        if name != self._last_name:
            self._last_name = name
            self._tune_count = 0

        if abs(reading.cents) <= self.epsilon_cents:
            self._tune_count += 1
            self._detune_counts[name] = 0
            if self._tune_count >= self.confirmation_frames and name not in self.tuned:
                self.tuned.add(name)
                self._tune_count = 0
                return "tuned"
            return None

        self._tune_count = 0
        if name in self.tuned and abs(reading.cents) >= self.detune_cents:
            self._detune_counts[name] = self._detune_counts.get(name, 0) + 1
            if self._detune_counts[name] >= self.detune_frames:
                self.tuned.remove(name)
                self._detune_counts[name] = 0
                return "detuned"
        else:
            self._detune_counts[name] = 0
        return None

    def reset_pending(self) -> None:
        self._last_name = None
        self._tune_count = 0

    def reset(self) -> None:
        """Start a completely fresh six-string tuning session."""
        self.tuned.clear()
        self._detune_counts.clear()
        self.reset_pending()

    @property
    def confirmation_progress(self) -> tuple[int, int]:
        return self._tune_count, self.confirmation_frames

    @property
    def string_progress(self) -> str:
        """Return an ordered, compact view of the six standard strings."""
        markers = (
            target.name[0] if target.name in self.tuned else "_"
            for target in STANDARD_TUNING
        )
        return f"[ {' '.join(markers)} ]"

    @property
    def complete(self) -> bool:
        return len(self.tuned) == len(STANDARD_TUNING)


def run_tuner(
    session: TuningSession,
    gauge: SevenLightGauge | NullLightRenderer | None = None,
    device: int | str | None = None,
    minimum_snr_db: float = 6.0,
    on_complete: Callable[[], None] | None = None,
) -> None:
    """Run a low-latency rolling-window standard guitar tuner."""
    session.reset()
    gauge = gauge or NullLightRenderer(epsilon_cents=session.epsilon_cents)
    blocks: queue.Queue[np.ndarray] = queue.Queue(maxsize=8)
    rolling = np.zeros(WINDOW_SIZE, dtype=np.float32)
    noise_floor = DynamicNoiseFloor()

    def callback(indata: np.ndarray, _frames: int, _time: object, _status: object) -> None:
        try:
            blocks.put_nowait(indata[:, 0].copy())
        except queue.Full:
            blocks.get_nowait()
            blocks.put_nowait(indata[:, 0].copy())

    total_strings = len(session.targets)
    print("Standard tuning: E2 A2 D3 G3 B3 E4. Press Ctrl+C to stop.")
    print(f"Tuned strings: 0/{total_strings} {session.string_progress}")
    previous_line_length = 0
    last_notebook_report = 0.0
    last_signal_time = time.monotonic()
    lights_are_on = False
    try:
        with sd.InputStream(
            device=device,
            channels=1,
            samplerate=SAMPLE_RATE,
            blocksize=HOP_SIZE,
            dtype="float32",
            callback=callback,
        ):
            while not session.complete:
                block = blocks.get()
                rolling[:-HOP_SIZE] = rolling[HOP_SIZE:]
                rolling[-HOP_SIZE:] = block
                analysis = analyze_block(rolling, noise_floor.value, minimum_snr_db=minimum_snr_db)
                reading = analysis.reading
                if reading is None:
                    noise_floor.observe(analysis.signal_rms)
                    session.reset_pending()
                    if (
                        lights_are_on
                        and time.monotonic() - last_signal_time
                        >= LIGHT_OFF_DELAY_SECONDS
                    ):
                        gauge.clear()
                        lights_are_on = False
                    continue

                gauge.render(reading.cents)
                lights_are_on = True
                last_signal_time = time.monotonic()
                transition = session.update(reading)
                state = transition.upper() if transition else reading.direction.upper()
                line = (
                    f"{reading.target.name:>2} {reading.frequency:7.2f} Hz "
                    f"{reading.cents:+6.1f} cents {state:>8} "
                    f"tuned {len(session.tuned)}/{total_strings} strings "
                    f"{session.string_progress}"
                )
                if sys.stdout.isatty():
                    print(
                        f"\r{line.ljust(previous_line_length)}",
                        end="",
                        flush=True,
                    )
                    previous_line_length = max(previous_line_length, len(line))
                else:
                    # Notebook output does not reliably process carriage
                    # returns. Use throttled complete lines so remnants such
                    # as "2/66" cannot be formed by merged stream updates.
                    now = time.monotonic()
                    if transition or now - last_notebook_report >= 0.5:
                        print(line, flush=True)
                        last_notebook_report = now
    finally:
        gauge.clear()

    print("\nAll six strings are tuned.")
    if on_complete:
        on_complete()
