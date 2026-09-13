# Five-LED Coupled Oscillator

ESP32/Arduino firmware for a Kuramoto-model demonstration. Three capacitive
electrodes recognize directional swipes, a fourth capacitive button chooses the
controlled parameter, and five PWM LEDs visualize the oscillator phases.

## Controls

| Mode | Forward swipe (`3 -> 2 -> 1`) | Reverse swipe (`1 -> 2 -> 3`) |
| --- | --- | --- |
| Frequency | Increase frequency | Decrease frequency |
| K | Increase coupling | Decrease coupling |

The mode button toggles between frequency and K. The selected mode and both
parameter values persist.

The speed-dependent change preserves the original program's formula:

```text
durationSeconds = swipeDurationMs / 1000
speedFactor = 1 / (1 + sqrt(durationSeconds))
change = swipeGain * speedFactor
```

## Files

| File | Responsibility |
| --- | --- |
| `CoupledOscillator.ino` | Short main loop and diagnostics |
| `Config.h` | All hardware and tuning values |
| `InputController.*` | Touch sensing, swipes, mode button, and cross-talk guard |
| `ParameterController.h` | Header-only mode and parameter adjustment logic |
| `KuramotoLedSystem.*` | Physics integration, order parameter, and LED PWM |

## Default pins

| Part | GPIO |
| --- | --- |
| Swipe electrode 1 | 27 |
| Swipe electrode 2 | 33 |
| Swipe electrode 3 | 32 |
| Mode button | 13 |
| LEDs 1-5 | 15, 2, 4, 16, 17 |

GPIO 2, 4, and 15 are ESP32 boot-strapping pins. If the LED circuitry prevents
booting, move those LEDs to ordinary output pins and edit `Config.h`.

## Arduino IDE

1. Keep all files in a folder named exactly `CoupledOscillator`.
2. Open `CoupledOscillator.ino`.
3. Select the correct ESP32 board and port.
4. Compile, upload, and open Serial Monitor at 115200 baud.

On Windows, use a simple path such as `C:\Arduino\CoupledOscillator`. Avoid
parentheses, ampersands, and unusual characters in the sketch path.
