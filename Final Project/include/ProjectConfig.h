#pragma once

#include <cstdint>

namespace ProjectConfig {

// Logical board configuration.
constexpr int GRID_WIDTH = 32;
constexpr int GRID_HEIGHT = 32;
constexpr uint8_t COLOR_COUNT = 6;
constexpr uint8_t DEFAULT_COLOR = 0;  // White.

// User-facing time configuration.
constexpr uint32_t COOLDOWN_TIME_SECONDS = 60;
constexpr uint32_t SESSION_TIMEOUT_SECONDS = 30;
constexpr uint32_t FLASH_INTERVAL_MS = 350;
constexpr uint32_t COOLDOWN_NOTICE_MS = 1500;

constexpr uint32_t COOLDOWN_TIME_MS = COOLDOWN_TIME_SECONDS * 1000UL;
constexpr uint32_t SESSION_TIMEOUT_MS = SESSION_TIMEOUT_SECONDS * 1000UL;

// Palette indices stored by PlaceStateMachine. OFF is defined by the core.
enum ColorIndex : uint8_t {
  WHITE = 0,
  RED,
  GREEN,
  BLUE,
  YELLOW,
  PURPLE
};

// Hardware placeholders. Replace every PIN_UNASSIGNED value after wiring.
constexpr int PIN_UNASSIGNED = -1;

constexpr int LED_STRIP_COUNT = 1;  // TODO: Replace with k.
constexpr int LED_DATA_PINS[LED_STRIP_COUNT] = {PIN_UNASSIGNED};
constexpr uint8_t LED_BRIGHTNESS = 64;

constexpr int PIN_X_KNOB = PIN_UNASSIGNED;
constexpr int PIN_Y_KNOB = PIN_UNASSIGNED;
constexpr int PIN_COLOR_UP_BUTTON = PIN_UNASSIGNED;
constexpr int PIN_COLOR_DOWN_BUTTON = PIN_UNASSIGNED;
constexpr int PIN_CONFIRM_BUTTON = PIN_UNASSIGNED;

constexpr int PIN_NFC_RX = PIN_UNASSIGNED;
constexpr int PIN_NFC_TX = PIN_UNASSIGNED;
constexpr uint32_t NFC_SERIAL_BAUD = 115200;

constexpr int PIN_DISPLAY_SDA = PIN_UNASSIGNED;
constexpr int PIN_DISPLAY_SCL = PIN_UNASSIGNED;
constexpr int DISPLAY_PRIMARY_ADDRESS = PIN_UNASSIGNED;
constexpr int DISPLAY_SECONDARY_ADDRESS = PIN_UNASSIGNED;

// Until the storage skeleton is filled, commits remain RAM-only and succeed.
constexpr bool PERSISTENCE_ENABLED = false;

}  // namespace ProjectConfig
