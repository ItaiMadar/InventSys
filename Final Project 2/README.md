# Final Project 2 — Physical r/place LED Grid

ESP32 firmware for a physical, interactive RGB LED pixel board. Users identify themselves with an RC522 RFID tag, select a pixel with two 10 kΩ knobs, cycle through colors using a button, and confirm placement. A small SSD1306 OLED displays prompts. Board state is stored in the ESP32's onboard NVS flash; RFID cooldowns reset after reboot.

## Arduino IDE (recommended)

Open **`Arduino/PhysicalRPlace/PhysicalRPlace.ino`** in Arduino IDE. Keep all the `.cpp` and `.h` files in that same `PhysicalRPlace` folder; Arduino IDE needs them to build the sketch.

Install the Espressif ESP32 boards package and these libraries:
- MFRC522 (Miguel Balboa)
- Adafruit NeoPixel
- Adafruit SSD1306
- Adafruit GFX Library

Select the correct ESP32 board and port, upload, and open Serial Monitor at **115200 baud**. See `docs/SETUP.md` for full instructions.

## Initial configuration

`Arduino/PhysicalRPlace/ProjectConfig.h` begins with:

```cpp
constexpr bool TEST_MODE = true;
```

This configures a **6 × 1 low-brightness test strip**. Once the full 17 × 17 (289-LED) grid has appropriate external 5V power and fused injection wiring, change it to `false` and upload again. The first LED data input is on **GPIO4**.

A serial `t` emulates an RFID card in test mode. A **short button press** cycles color; holding it for **850 ms** confirms placement. The physical circuit currently has one button, not the three buttons in the original concept.

## Directory layout

- `Arduino/PhysicalRPlace/` — complete Arduino IDE firmware and source headers.
- `PlatformIO/` — equivalent source tree and `platformio.ini` for PlatformIO.
- `hardware/` — updated Fritzing two-potentiometer diagram and pinout reference; the LED connection on GPIO4 is a subsequent addition and may not appear in the Fritzing drawing.
- `tests/` — offline C++ state-machine tests.
- `docs/SETUP.md` — detailed wiring and operating instructions.
- `docs/projectVision.md` — original project vision; it describes aspirational features, not all of which are implemented.

## ESP32 pin map

| Component | Signal | GPIO |
|---|---|---:|
| RC522 RFID | SS, RST, SCK, MOSI, MISO | 27, 26, 18, 23, 19 |
| SSD1306 OLED | SDA, SCL | 21, 22 |
| X/Y potentiometers | wipers | 34, 35 |
| Pushbutton | active LOW | 32 |
| SK6812 RGB | DIN (level shifter recommended) | 4 |
| L293D Motor A | ENABLE, IN1, IN2 | 25, 14, 13 |
| L293D Motor B | ENABLE, IN3, IN4 | 33, 16, 17 |

The motor driver is initialized in an inactive state; there is no implemented haptic feedback behavior yet.

## Fixed compilation issue

The Adafruit SSD1306 library declares `WHITE` as a macro. Earlier firmware used `ProjectConfig::WHITE`, which cannot coexist with that macro. This version consistently uses `ProjectConfig::COLOR_WHITE` in **both** `ProjectConfig.h` and `HardwareAdapter.cpp`, in the Arduino and PlatformIO variants. Do not mix files from the earlier archive with this folder.

## Safety and verification

The ELEGOO MB V2 and a rectangular 9V battery are suitable only for limited low-brightness tests with a few LEDs. Never power the 289-LED matrix from the MB V2, ESP32 5V pin or breadboard rails. For the complete grid, use an appropriately rated **5V regulated power supply**, current-limited and separately fused power feeds, and common ground. See `docs/SETUP.md`.

Run host state-machine tests with `sh tests/run_native_tests.sh` from this folder. The complete firmware has **not** been built against your local Arduino ESP32 library installation or verified on physical hardware, so upload and test it before depending on it for an event or demo.
