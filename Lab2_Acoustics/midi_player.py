"""MIDI parsing and monophonic buzzer playback."""

from __future__ import annotations

import math
import time
from dataclasses import dataclass
from pathlib import Path

import mido

from controller import BuzzerController


@dataclass(frozen=True)
class Segment:
    note: int | None
    duration: float


def midi_note_to_frequency(note: int) -> int:
    return round(440.0 * math.pow(2.0, (note - 69) / 12.0))


def parse_midi(path: Path) -> list[Segment]:
    """Convert MIDI events into monophonic note/rest segments."""
    active: set[tuple[int, int]] = set()
    segments: list[Segment] = []

    def current_note() -> int | None:
        return max((note for _, note in active), default=None)

    def append(note: int | None, duration: float) -> None:
        if duration <= 0:
            return
        if segments and segments[-1].note == note:
            previous = segments[-1]
            segments[-1] = Segment(note, previous.duration + duration)
        else:
            segments.append(Segment(note, duration))

    for message in mido.MidiFile(path):
        append(current_note(), message.time)
        key = (getattr(message, "channel", 0), getattr(message, "note", -1))
        if message.type == "note_on" and message.velocity > 0:
            active.add(key)
        elif message.type == "note_off" or (
            message.type == "note_on" and message.velocity == 0
        ):
            active.discard(key)
    return segments


def play_midi(
    controller: BuzzerController,
    midi_path: Path,
    max_seconds: float | None = None,
) -> None:
    segments = parse_midi(midi_path)
    total = sum(segment.duration for segment in segments)
    print(f"Playing {midi_path.name}: {len(segments)} segments, {total:.1f} seconds")
    started = time.monotonic()
    try:
        for index, segment in enumerate(segments, start=1):
            if max_seconds is not None and time.monotonic() - started >= max_seconds:
                break
            frequency = 0 if segment.note is None else midi_note_to_frequency(segment.note)
            controller.play_frequency(frequency, segment.duration)
            if segment.note is not None:
                print(
                    f"\rSegment {index}/{len(segments)}: {frequency} Hz",
                    end="",
                    flush=True,
                )
    finally:
        controller.stop()
    print("\nPlayback complete.")
