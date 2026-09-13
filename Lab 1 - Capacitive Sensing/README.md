# Five-LED Coupled-Oscillator

ESP32/Arduino firmware for a physical Kuramoto-model demonstration. Three
capacitive electrodes detect swipes, a fourth capacitive button selects the
controlled parameter, and five PWM LEDs visualize coupled phase oscillators.

## Controls

The mode button toggles between frequency mode and coupling mode. The selected
mode remains active until the button is pressed again.

| Selected mode | Forward swipe (`1 -> 2 -> 3`) | Reverse swipe (`3 -> 2 -> 1`) |
| --- | --- | --- |
| Frequency | Increase oscillator frequency | Decrease oscillator frequency |
| K | Increase coupling strength | Decrease coupling strength |

Each swipe acts directly on the current value. Faster swipes make larger
changes:

```text
frequency change = FrequencySwipeGain / swipeDurationSeconds
K change         = CouplingSwipeGain / swipeDurationSeconds
```

The frequency and K values are constrained to the ranges in `Config.h`.

## Project structure

| File | Responsibility |
| --- | --- |
| `CoupledOscillatorDemo.ino` | Coordinates buttons, swipes, parameters, and LEDs |
| `Config.h` | Pins, thresholds, gains, and parameter ranges |
| `TouchButton.*` | Derivative detector used by each swipe electrode |
| `SwipeDetector.*` | Recognizes valid three-electrode sequences |
| `ModeButton.*` | Debounced press/release detector for the selector button |
| `ParameterController.*` | Stores mode, frequency, and K and applies swipes |
| `KuramotoLedSystem.*` | Integrates the oscillator equations and drives PWM |

## Default wiring

| Part | GPIO |
| --- | --- |
| Swipe electrode 1 | 27 |
| Swipe electrode 2 | 33 |
| Swipe electrode 3 | 32 |
| Mode button | 13 |
| LEDs 1-5 | 15, 2, 4, 16, 17 |

Make the mode button a physically separate foil or copper-tape pad and cover it
with a thin insulating layer. Leave it untouched when the ESP32 starts so the
software can establish its baseline.

Connect each ordinary LED through its own 220-470 ohm resistor. High-current
lamps or LED strips require transistor/MOSFET drivers and an appropriate power
supply with a shared ground.

GPIO 2, 4, and 15 are ESP32 boot-strapping pins. If the LED circuitry interferes
with booting, move the LEDs to safer output pins such as 18, 19, 21, 22, and 23.
Verify all touch-pin assignments against the exact ESP32 variant.

## Button calibration

Serial output includes `mode_raw` and `mode_amount`. While untouched,
`mode_amount` should stay near zero. A press must cross:

```cpp
constexpr float kModePressThreshold = 30.0f;
```

and the button must return below this value before it can trigger again:

```cpp
constexpr float kModeReleaseThreshold = 15.0f;
```

If a press produces a negative `mode_amount`, reverse:

```cpp
constexpr bool kModePressIsRising = false;
```

Swipe activity is prevented from toggling the mode: a mode event is rejected
while a swipe is incomplete and for `kModeSwipeGuardMs` after any swipe-pad
activation. The mode pad must also remain pressed for `kModeHoldMs` (250 ms by
default), preventing a brief cross-talk transient from changing modes.

## Arduino IDE

1. Extract the archive completely.
2. Keep all files in a folder named exactly `CoupledOscillatorDemo`.
3. Use a simple Windows path such as `C:\Arduino\CoupledOscillatorDemo`.
4. Open `CoupledOscillatorDemo.ino` and select the exact ESP32 board and port.
5. Compile, upload, and open Serial Monitor at 115200 baud.

Avoid parentheses, ampersands, or unusual characters in the sketch path.
