#pragma once

// Pin assignments from the supplied Fritzing 2-potentiometer diagram.
// constexpr assignments are global; all hardware initialization is in begin().
namespace HardwarePins {
constexpr int RFID_SS = 27;
constexpr int RFID_RST = 26;
constexpr int RFID_SCK = 18;
constexpr int RFID_MOSI = 23;
constexpr int RFID_MISO = 19;

constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;

constexpr int CONFIRM_BUTTON = 32;
constexpr int POSITION_X = 34;
constexpr int POSITION_Y = 35;

constexpr int MOTOR_A_ENABLE = 25;
constexpr int MOTOR_A_IN1 = 14;
constexpr int MOTOR_A_IN2 = 13;
constexpr int MOTOR_B_ENABLE = 33;
constexpr int MOTOR_B_IN1 = 16;
constexpr int MOTOR_B_IN2 = 17;

// Proposed LED data connection, NOT part of the original Fritzing diagram.
constexpr int LED_DATA = 4;
}
