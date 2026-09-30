"""Run the standard guitar tuner and play die_forelle when complete."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

import serial
import sounddevice as sd

from controller import DEFAULT_FQBN, BuzzerController, choose_port, upload_firmware
from tuner import TuningSession, run_tuner
from light_renderer import SevenLightGauge
from midi_player import play_midi

ROOT = Path(__file__).parent
DEFAULT_SKETCH = ROOT /  "arduinoController"
COMPLETION_MIDI = ROOT / "die_forelle.mid"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Tune a standard six-string guitar.")
    parser.add_argument("--epsilon", type=float, default=5.0)
    parser.add_argument("--snr-db", type=float, default=6.0)
    parser.add_argument("--device", help="Microphone device index or name.")
    parser.add_argument("--list-devices", action="store_true")
    parser.add_argument("--port")
    parser.add_argument("--fqbn", default=DEFAULT_FQBN)
    parser.add_argument("--sketch", type=Path, default=DEFAULT_SKETCH)
    parser.add_argument("--arduino-cli", type=Path)
    parser.add_argument("--no-upload", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.list_devices:
        print(sd.query_devices())
        return 0
    device: int | str | None = args.device
    if isinstance(device, str) and device.isdigit():
        device = int(device)
    try:
        port = choose_port(args.port)
        if not args.no_upload:
            upload_firmware(port, args.sketch, args.fqbn, args.arduino_cli)
        with BuzzerController(port) as controller:
            run_tuner(
                TuningSession(epsilon_cents=args.epsilon),
                gauge=SevenLightGauge(controller, epsilon_cents=args.epsilon),
                device=device,
                minimum_snr_db=args.snr_db,
                on_complete=lambda: play_midi(controller, COMPLETION_MIDI),
            )
    except subprocess.CalledProcessError as exc:
        print(f"Arduino compile/upload failed ({exc.returncode}).", file=sys.stderr)
        return 1
    except (RuntimeError, serial.SerialException, sd.PortAudioError, OSError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nTuner stopped.")
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
