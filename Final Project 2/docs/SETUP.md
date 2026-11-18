# Physical r/place -- ESP32 Arduino firmware

This package integrates the original project source files with the supplied
ESP32 pinout and firmware updates. It keeps the original
`PlaceStateMachine.cpp` algorithm unchanged, reconstructs its missing header,
and completes its ESP32 hardware adapter.

## Start here: Arduino IDE

1. Open `Arduino/PhysicalRPlace/PhysicalRPlace.ino` in Arduino IDE.
2. Install **ESP32 by Espressif Systems** from Boards Manager and select your
   actual ESP32 board (generic ESP32 Dev Module may work for common dev kits).
3. From Library Manager install: **MFRC522** (Miguel Balboa), **Adafruit NeoPixel**,
   **Adafruit SSD1306**, and **Adafruit GFX Library**. SPI, Wire, and Preferences
   come with the ESP32 framework.
4. Open `ProjectConfig.h`. It ships with **`TEST_MODE = true`**, a 6x1 layout
   with low LED brightness for your **six-LED bench test**.
5. Connect the hardware following the pin table below. Upload the sketch.
6. Open Serial Monitor at **115200 baud**. Send `t` to simulate a card, or tap a
   physical RC522 RFID tag. The serial simulation is enabled only in test mode.
7. Turn the two 10k pots to select X and Y. On the six-LED test, Y is fixed.
   **Tap GPIO32 button** to cycle White/Red/Green/Blue/Yellow/Purple;
   **hold for 850ms** to place the pixel. A placed pixel is saved into ESP32
   NVS and restores after reboot. The UID has a 60-second cooldown in RAM.
8. Once the six-LED test works, replace the test wiring with the completed
   **17x17 serpentine grid**, use a dedicated 5V power supply with correctly
   sized fused power-injection wiring, then set `TEST_MODE = false` and reupload.
   The six-pixel snapshot is ignored when switching to a 289-pixel grid.

## Physical pins

| Part | Signal | ESP32 GPIO |
|---|---|---|
| RC522 (3.3V only) | SDA / SS | 27 |
| RC522 | RST | 26 |
| RC522 | SCK | 18 |
| RC522 | MOSI | 23 |
| RC522 | MISO | 19 |
| SSD1306 OLED | SDA | 21 |
| SSD1306 OLED | SCL | 22 |
| X dial 10k (wiper) | ADC1 | 34 |
| Y dial 10k (wiper) | ADC1 | 35 |
| Single pushbutton to GND | INPUT_PULLUP | 32 |
| L293D A | EN / IN1 / IN2 | 25 / 14 / 13 |
| L293D B | EN / IN3 / IN4 | 33 / 16 / 17 |
| SK6812 ECO RGB | DIN via 74AHCT125 recommended | **4** |

These are constant pin assignments in `HardwarePins.h`, not macros. All
`pinMode`, SPI, I2C, ADC, and LED initialization is in `HardwareAdapter::begin()`.

## Important changes relative to the source documents

- The **RC522 is SPI**, so the source's serial NFC RX/TX placeholders were
  replaced with the actual MFRC522 reader library and connections.
- Your Fritzing design has **one button**, while the project vision originally
  called for *two color buttons and a confirm button*. To make the current
  hardware usable, **short press cycles forward**, **hold >=850ms confirms**.
  Colors wrap around; no extra GPIO or buttons are silently assumed.
- Your provided `ProjectConfig.h` originally configured **32 x 32** and left
  all GPIO assignments blank. This version configures **6x1 test / 17x17 final**.
- The motor-driver GPIOs are configured and held disabled at startup. Motor
  feedback is not triggered by placement. Their control/power remain wired as
  the original Fritzing schematic specifies.
- Your project vision requested durable board state. This version enables
  **two-slot, versioned, CRC32-checked NVS snapshots**, updated only on commit.
  The previous valid snapshot remains in the other slot while a new one saves.
  **Cooldowns are RAM-only** and reset at reboot, as specified in the vision.
- The OLED driver expects a **128x64 SSD1306** at I2C `0x3C` or `0x3D`.
  If no such display is found, the board still runs and logs through Serial.
- A physical 17x17 grid is assumed to use **one data chain** with **17 cut
  rows of 17 LEDs**, serpentine starting at the top-left, first row going
  left-to-right. Invert `FIRST_ROW_LEFT_TO_RIGHT` if wired oppositely.
- Only **palette indices** are saved per coordinate, consistent with the
  project vision, not arbitrary 24-bit user-selected RGB or event timestamps.

## Safe power wiring

The **9V rectangular battery + ELEGOO MB V2** combination is acceptable only
for a few LEDs at reduced brightness (ESP32 powered separately via USB).
Do not power 289 LEDs from the MB V2, a small 9V battery, an ESP32 GPIO, or a
breadboard rail. Use an enclosed, safety-certified regulated **5V supply** sized
for the full LED load. The 300 LED product is rated up to **90W** (~18A at 5V).
A 5V/25A supply provides headroom for the LEDs and controller if correctly
fused and distributed. Route power through correctly sized wires and several
power-injection points; never through the ESP32 or the entire thin strip.

For bench testing:
- SK6812 **+5V -> MB V2 regulated +5V**, **GND -> MB V2 GND**.
- **ESP32 USB -> a separate computer/5V USB source**.
- **ESP32 GND -> LED/MB V2 GND** (mandatory common reference).
- **ESP32 GPIO4 -> level shifter -> 330ohm resistor -> strip DIN**.
- Observe signal direction. Never connect 9V directly to SK6812.
- If you don't yet have a level shifter, a *short-wire* direct 3.3V data test
  may work but is not guaranteed by the datasheet.

## Functional details

- Six colors: White, Red, Green, Blue, Yellow, Purple.
- Preview alternates between selected palette color and the saved color under
  the chosen cell at 350ms intervals.
- Knob ADC is filtered and uses 25-count quantization hysteresis.
- Button presses are debounced (35ms) and read without `delay()`.
- LED output is skipped unless a frame actually changes. Full 289-pixel
  transfers still take about 9ms at the 800kHz protocol rate.
- OLED updates are reduced to avoid unnecessary I2C traffic.
- Valid snapshots require matching board dimensions and color palette count;
  an incompatible snapshot results in an empty board, not corrupt data.
- No Wi-Fi, external RTC, touchscreen, or timestamped placement log is added.
  Those are beyond the uploaded project's first-demo behavior.

## PlatformIO

An equivalent project is in `PlatformIO/`, with the original project layout
(`src`, `include`, `platformio.ini`). Run `pio run`, `pio run --target upload`,
`pio device monitor` after installing PlatformIO and its dependencies.

## Verification and limitations

- Reconstructed `PlaceStateMachine.h` to match the supplied .cpp public and
  private functions and kept the supplied state-machine implementation.
- Offline C++ compilation and host tests are included in `tests/`.
- **The real ESP32 toolchain and Arduino libraries were not available in the
  authoring environment**, so an actual firmware upload and hardware test are
  still required. Library install/version behavior can be board-specific.
- If motor-driver supply or OLED pinout differs from your Fritzing diagram,
  confirm electrically before applying power.

## Compile fix (SSD1306 WHITE macro)

The `Adafruit_SSD1306` library defines `WHITE` as a preprocessor macro.
To avoid a collision with the project palette enum, the white palette
constant is named `ProjectConfig::COLOR_WHITE` in both the Arduino IDE
and PlatformIO versions. This has no effect on saved palette indices.
