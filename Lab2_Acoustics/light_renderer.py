"""Seven-light tuning gauge rendering."""

from __future__ import annotations

from dataclasses import dataclass, field

import serial

from controller import BuzzerController


def gauge_position(
    cents: float, epsilon_cents: float, delta_multiplier: float
) -> int:
    """Map pitch error to seven LEDs using epsilon-based bands."""
    if epsilon_cents <= 0:
        raise ValueError("epsilon_cents must be positive.")
    if delta_multiplier <= 2:
        raise ValueError("delta_multiplier must be greater than 2.")
    error = abs(cents)
    if error <= epsilon_cents:
        magnitude = 0
    elif error <= 2.0 * epsilon_cents:
        magnitude = 1
    elif error <= delta_multiplier * epsilon_cents:
        magnitude = 2
    else:
        magnitude = 3
    return -magnitude if cents < 0 else magnitude


@dataclass
class SevenLightGauge:
    """Map pitch error to seven LEDs: -3 is flat, 0 tuned, +3 sharp."""

    controller: BuzzerController
    epsilon_cents: float = 5.0
    delta_multiplier: float = 3.0
    _last_position: int | None = field(default=None, init=False)
    _warned: bool = field(default=False, init=False)

    def position_for_cents(self, cents: float) -> int:
        return gauge_position(cents, self.epsilon_cents, self.delta_multiplier)

    def render(self, cents: float) -> int:
        position = self.position_for_cents(cents)
        if position == self._last_position:
            return position
        try:
            self.controller.set_gauge(position)
            self._last_position = position
            self._warned = False
        except (RuntimeError, serial.SerialException) as exc:
            # A light is feedback only; losing one update must not stop audio
            # analysis or discard the current tuning session.
            self._last_position = None
            if not self._warned:
                print(f"\nLight update skipped: {exc}")
                self._warned = True
        return position

    def clear(self) -> None:
        try:
            self.controller.clear_lights()
        except (RuntimeError, serial.SerialException):
            pass
        self._last_position = None


@dataclass
class NullLightRenderer:
    """No-op implementation useful when the LED hardware is unavailable."""

    epsilon_cents: float = 5.0
    delta_multiplier: float = 3.0

    def render(self, cents: float) -> int:
        return gauge_position(cents, self.epsilon_cents, self.delta_multiplier)

    def clear(self) -> None:
        pass
