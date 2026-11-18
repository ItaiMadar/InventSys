# ESP32 Complete Diagnostic Circuit - Pinout (with second 10k potentiometer)

Pinout extracted from `ESP32_Complete_Diagnostic_Circuit.fzz` (Fritzing project) and modified in `ESP32_Complete_Diagnostic_Circuit_2Pots.fzz`.

## ESP32 pin mappings

| Component | Component pin / function | ESP32 GPIO | Notes |
|---|---|---|---|
| MFRC522 RFID | SDA / SS (chip select) | GPIO27 | SPI |
| MFRC522 RFID | SCK | GPIO18 | SPI clock |
| MFRC522 RFID | MOSI | GPIO23 | SPI master out |
| MFRC522 RFID | MISO | GPIO19 | SPI master in |
| MFRC522 RFID | RST | GPIO26 | Reset |
| 0.96-inch OLED | SDA | GPIO21 | I2C data |
| 0.96-inch OLED | SCL | GPIO22 | I2C clock |
| Pushbutton | A1 | GPIO32 | B1 to ground; configure INPUT_PULLUP |
| Potentiometer 1 (10k) | Wiper | GPIO34 | Analog ADC1 input |
| **Potentiometer 2 (10k; added)** | **Wiper** | **GPIO35** | **Analog ADC1 input** |
| L293D | Pin 1 EN1 | GPIO25 | Motor A enable |
| L293D | Pin 2 IN1 | GPIO14 | Motor A input 1 |
| L293D | Pin 7 IN2 | GPIO13 | Motor A input 2 |
| L293D | Pin 9 EN2 | GPIO33 | Motor B enable |
| L293D | Pin 10 IN3 | GPIO16 | Motor B input 1 |
| L293D | Pin 15 IN4 | GPIO17 | Motor B input 2 |

## Power wiring

| Component | Power / ground connections |
|---|---|
| 9V battery | + -> ELEGOO MB-V2 9V_IN+, - -> MB-V2 9V_IN- |
| ELEGOO MB-V2 | Regulated 5V_OUT -> 5V power rail; GND_OUT -> common ground |
| ESP32 | VIN -> 5V rail; GND_L and GND_R -> common ground; 3V3 -> 3.3V rail |
| MFRC522 RFID | 3.3V -> 3.3V rail; GND -> ground; IRQ not wired |
| OLED | VCC -> 3.3V rail; GND -> ground |
| Potentiometer 1 | Outer terminals -> 3.3V and GND; center wiper -> GPIO34 |
| **Potentiometer 2** | **Outer terminals -> 3.3V and GND; center wiper -> GPIO35** |
| Pushbutton | B1 -> ground |
| L293D | Pin 16 VCC1 -> 5V; pin 8 VCC2 -> 5V; pins 4, 5, 12, 13 -> ground |
| C1 (100 nF) | 5V -> ground |
| C2 (100 uF) | 5V -> ground, observe polarity |
| C3 (100 nF) | 3.3V -> ground |

## Motor wiring

| Motor | Driver outputs |
|---|---|
| Vibration motor A | M+ -> L293D pin 3 OUT1; M- -> L293D pin 6 OUT2 |
| Vibration motor B | M+ -> L293D pin 11 OUT3; M- -> L293D pin 14 OUT4 |

## Added potentiometer details

- A second instance of the circuit's existing 10k potentiometer custom part is added at the left side of the diagrams.
- Pin 1 (3V3) -> 3.3V power rail P5.
- Pin 2 (WIPER) -> ESP32 GPIO35 (ADC1).
- Pin 3 (GND) -> common ground rail P15.
- Connections are present in the breadboard, schematic, and PCB views of the modified `.fzz` file.
- ESP32 GPIO34 and GPIO35 are **input-only pins**, which is suitable for potentiometer readings.
- Do not connect a potentiometer outer terminal to 5V: the ESP32 ADC input must stay within the 3.3V logic domain.

## File verification note

The edited Fritzing archive is a valid ZIP-based `.fzz`; the circuit's connector references were checked to be reciprocal across all three views. The Fritzing desktop GUI was not available in this environment to visually open and save the file, so verify placement and wire rendering after opening it in Fritzing.
