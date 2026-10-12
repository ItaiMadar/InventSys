"""Convert chess move evaluations into motor frequency and amplitude."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Sequence

from engine import MoveEvaluation, normalize_expected_scores


@dataclass(frozen=True)
class HapticSettings:
    top_n: int = 5
    frequency_weak_hz: float = 2.0
    frequency_strong_hz: float = 10.0
    amplitude_weak: float = 0.15
    amplitude_strong: float = 1.0

    def validate(self) -> None:
        if self.top_n < 1:
            raise ValueError("top_n must be at least 1")
        if not 0.5 <= self.frequency_weak_hz <= 20.0:
            raise ValueError("weak frequency must be between 0.5 and 20 Hz")
        if not self.frequency_weak_hz <= self.frequency_strong_hz <= 20.0:
            raise ValueError("strong frequency must be at least the weak frequency and at most 20 Hz")
        if not 0.0 <= self.amplitude_weak <= 1.0:
            raise ValueError("weak amplitude must be between 0 and 1")
        if not self.amplitude_weak <= self.amplitude_strong <= 1.0:
            raise ValueError("strong amplitude must be at least the weak amplitude and at most 1")


@dataclass(frozen=True)
class HapticSignal:
    quality: float
    frequency_hz: float
    amplitude: float


def signal_for_quality(quality: float, settings: HapticSettings) -> HapticSignal:
    settings.validate()
    quality = max(0.0, min(1.0, quality))
    return HapticSignal(
        quality=quality,
        frequency_hz=settings.frequency_weak_hz
        + quality * (settings.frequency_strong_hz - settings.frequency_weak_hz),
        amplitude=settings.amplitude_weak
        + quality * (settings.amplitude_strong - settings.amplitude_weak),
    )


def build_move_signals(
    evaluations: Sequence[MoveEvaluation], settings: HapticSettings
) -> dict[str, HapticSignal]:
    """Return signals for only the configured top-N moves."""

    settings.validate()
    candidates = list(evaluations[: settings.top_n])
    qualities = normalize_expected_scores(
        [evaluation.expected_score for evaluation in candidates]
    )
    return {
        evaluation.move: signal_for_quality(quality, settings)
        for evaluation, quality in zip(candidates, qualities, strict=True)
    }


def build_piece_signals(
    evaluations: Sequence[MoveEvaluation], settings: HapticSettings
) -> dict[str, HapticSignal]:
    """Give each piece the strongest destination signal among its legal moves."""

    settings.validate()
    move_signals = build_move_signals(evaluations, settings)
    best_qualities = {evaluation.move[:2]: 0.0 for evaluation in evaluations}
    for evaluation in evaluations:
        move_signal = move_signals.get(evaluation.move)
        if move_signal is None:
            continue
        origin = evaluation.move[:2]
        best_qualities[origin] = max(
            best_qualities[origin],
            move_signal.quality,
        )

    return {
        origin: signal_for_quality(quality, settings)
        for origin, quality in best_qualities.items()
    }
