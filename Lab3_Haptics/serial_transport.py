"""Thread-safe serial transport for the vibration-controller firmware."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass

import serial
from serial.tools import list_ports

from haptics import HapticSignal


@dataclass(frozen=True)
class SerialPortInfo:
    device: str
    description: str

    @property
    def label(self) -> str:
        return f"{self.device} — {self.description}"


def available_ports() -> list[SerialPortInfo]:
    return [
        SerialPortInfo(port.device, port.description or "Serial device")
        for port in sorted(list_ports.comports(), key=lambda item: item.device)
    ]


class SerialVibrationController:
    def __init__(self, baud_rate: int = 115200) -> None:
        self.baud_rate = baud_rate
        self._serial: serial.Serial | None = None
        self._lock = threading.Lock()

    @property
    def connected(self) -> bool:
        with self._lock:
            return self._serial is not None and self._serial.is_open

    def connect(self, port: str, reset_wait_seconds: float = 2.0) -> None:
        connection = serial.Serial(
            port=port,
            baudrate=self.baud_rate,
            timeout=0.1,
            write_timeout=0.5,
        )
        try:
            # Most Arduino-style boards reset when the serial port opens.
            time.sleep(reset_wait_seconds)
            connection.reset_input_buffer()
        except Exception:
            connection.close()
            raise

        with self._lock:
            if self._serial is not None:
                self._serial.close()
            self._serial = connection

    def _write(self, command: str) -> None:
        with self._lock:
            if self._serial is None or not self._serial.is_open:
                return
            self._serial.write((command + "\n").encode("ascii"))
            self._serial.flush()

    def vibrate(self, signal: HapticSignal) -> None:
        self._write(f"VIB {signal.frequency_hz:.3f} {signal.amplitude:.3f} 0")

    def stop(self) -> None:
        self._write("STOP")

    def close(self) -> None:
        with self._lock:
            if self._serial is None:
                return
            try:
                if self._serial.is_open:
                    self._serial.write(b"STOP\n")
                    self._serial.flush()
            finally:
                self._serial.close()
                self._serial = None
