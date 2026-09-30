"""ESP32/Arduino detection, firmware upload, and buzzer communication."""

from __future__ import annotations

import os
import shutil
import subprocess
import time
from pathlib import Path

import serial
from serial.tools import list_ports

DEFAULT_BAUD_RATE = 115200
DEFAULT_FQBN = "esp32:esp32:esp32"
USB_MARKERS = ("arduino", "usb", "cp210", "ch340", "esp32")


def available_ports() -> list:
    return list(list_ports.comports())


def choose_port(requested: str | None = None) -> str:
    """Use an explicit port or auto-detect one USB controller."""
    if requested:
        return requested
    ports = available_ports()
    usb_ports = [
        port for port in ports
        if port.vid is not None
        or any(marker in port.description.lower() for marker in USB_MARKERS)
    ]
    if len(usb_ports) == 1:
        return usb_ports[0].device
    descriptions = "\n".join(
        f"  {port.device}: {port.description}" for port in ports
    ) or "  (none found)"
    if not usb_ports:
        raise RuntimeError(
            "No USB Arduino/ESP32 serial port was detected. Available ports:\n"
            f"{descriptions}\nConnect the controller or pass --port COMx."
        )
    raise RuntimeError(
        "More than one USB controller was detected. Choose one with --port COMx:\n"
        f"{descriptions}"
    )


def find_arduino_cli(requested: Path | None = None) -> Path:
    """Locate Arduino CLI, including the copy bundled with Arduino IDE 2."""
    if requested:
        cli = requested.expanduser().resolve()
        if cli.is_file():
            return cli
        raise RuntimeError(f"Arduino CLI was not found at: {cli}")
    command = shutil.which("arduino-cli")
    if command:
        return Path(command)
    local = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local"))
    bundled = local / "Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
    if bundled.is_file():
        return bundled
    raise RuntimeError(
        "Arduino CLI was not found. Install Arduino IDE 2 or pass --arduino-cli."
    )


def upload_firmware(
    port: str,
    sketch: Path,
    fqbn: str = DEFAULT_FQBN,
    arduino_cli: Path | None = None,
) -> None:
    """Compile an Arduino sketch and upload it to the selected controller."""
    sketch = sketch.resolve()
    sketch_file = sketch / f"{sketch.name}.ino"
    if not sketch_file.is_file():
        raise RuntimeError(f"Arduino sketch was not found: {sketch_file}")
    cli = find_arduino_cli(arduino_cli)
    print(f"Compiling {sketch_file.name} for {fqbn}...")
    subprocess.run([str(cli), "compile", "--fqbn", fqbn, str(sketch)], check=True)
    print(f"Uploading firmware to {port}...")
    subprocess.run(
        [str(cli), "upload", "--fqbn", fqbn, "--port", port, str(sketch)],
        check=True,
    )
    print("Firmware upload complete.")


class BuzzerController:
    """Serial client for the BuzzerPlayTone firmware."""

    def __init__(self, port: str, baud_rate: int = DEFAULT_BAUD_RATE) -> None:
        self.port = port
        self.baud_rate = baud_rate
        self.serial: serial.Serial | None = None

    def __enter__(self) -> BuzzerController:
        self.open()
        return self

    def __exit__(self, *_exc_info: object) -> None:
        self.close()

    def open(self) -> None:
        self.serial = serial.Serial(
            self.port, self.baud_rate, timeout=10.0, write_timeout=2.0
        )
        time.sleep(2.0)  # Opening serial resets most Arduino/ESP32 boards.
        self.wait_until_ready()

    def close(self) -> None:
        if self.serial is None:
            return
        if self.serial.is_open:
            try:
                try:
                    self.stop()
                except (RuntimeError, serial.SerialException):
                    pass
            finally:
                self.serial.close()
        self.serial = None

    def wait_until_ready(self, timeout: float = 8.0) -> None:
        controller = self._require_open()
        deadline = time.monotonic() + timeout
        controller.reset_input_buffer()
        while time.monotonic() < deadline:
            controller.write(b"PING\n")
            controller.flush()
            if controller.readline().decode("ascii", errors="replace").strip() == "READY":
                return
            time.sleep(0.1)
        raise RuntimeError("The controller did not respond. Check its firmware and port.")

    def play_frequency(self, frequency: int, duration_seconds: float) -> None:
        remaining_ms = max(1, round(duration_seconds * 1000))
        while remaining_ms:
            duration_ms = min(remaining_ms, 60000)
            self._request(f"{frequency},{duration_ms}", "DONE")
            remaining_ms -= duration_ms

    def stop(self) -> None:
        self._request("STOP", "DONE")

    def set_gauge(self, position: int) -> None:
        """Light one of seven gauge LEDs using a position from -3 to +3."""
        if position < -3 or position > 3:
            raise ValueError("Gauge position must be between -3 and 3.")
        self._request(f"LED,{position}", "DONE")

    def clear_lights(self) -> None:
        self._request("LEDS_OFF", "DONE")

    def _request(self, command: str, expected: str, attempts: int = 2) -> None:
        controller = self._require_open()
        last_response = ""
        for _attempt in range(attempts):
            controller.reset_input_buffer()
            controller.write(f"{command}\n".encode("ascii"))
            controller.flush()
            # Ignore boot noise and damaged lines, but stop as soon as the
            # firmware's expected acknowledgement arrives.
            for _line in range(4):
                raw = controller.readline()
                if not raw:
                    break
                response = raw.decode("ascii", errors="ignore").strip()
                if response == expected:
                    return
                if response:
                    last_response = response
                if response.startswith("ERROR"):
                    break
        raise RuntimeError(
            f"Controller did not acknowledge {command!r}; last response: "
            f"{last_response!r}"
        )

    def _require_open(self) -> serial.Serial:
        if self.serial is None or not self.serial.is_open:
            raise RuntimeError("The controller serial connection is not open.")
        return self.serial
