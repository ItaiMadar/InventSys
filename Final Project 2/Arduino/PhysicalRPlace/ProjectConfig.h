#pragma once

#include <cstdint>

namespace ProjectConfig {
// IMPORTANT: Starts in safe 6-LED prototype mode. Switch to false ONLY after
// installing an external, correctly fused 5V grid supply and power injection.
constexpr bool TEST_MODE = true;
constexpr int GRID_WIDTH = TEST_MODE ? 6 : 17;
constexpr int GRID_HEIGHT = TEST_MODE ? 1 : 17;
constexpr int LED_COUNT = GRID_WIDTH * GRID_HEIGHT;
constexpr int LED_STRIP_COUNT = 1;
constexpr uint8_t LED_BRIGHTNESS = TEST_MODE ? 20 : 40; // Scale 0..255

constexpr uint8_t COLOR_COUNT = 6;
constexpr uint8_t DEFAULT_COLOR = 0;
enum ColorIndex : uint8_t { COLOR_WHITE=0, RED, GREEN, BLUE, YELLOW, PURPLE };
constexpr uint32_t COOLDOWN_TIME_SECONDS = 60;
constexpr uint32_t SESSION_TIMEOUT_SECONDS = 30;
constexpr uint32_t COOLDOWN_TIME_MS = COOLDOWN_TIME_SECONDS * 1000UL;
constexpr uint32_t SESSION_TIMEOUT_MS = SESSION_TIMEOUT_SECONDS * 1000UL;
constexpr uint32_t FLASH_INTERVAL_MS = 350;
constexpr uint32_t COOLDOWN_NOTICE_MS = 1500;

// 1 wired button: tap = next color, hold >=850 ms = confirm.
constexpr uint32_t BUTTON_DEBOUNCE_MS = 35;
constexpr uint32_t BUTTON_CONFIRM_HOLD_MS = 850;
constexpr uint16_t ADC_HYSTERESIS = 25; // raw ESP32 ADC counts

constexpr bool PERSISTENCE_ENABLED = true;
constexpr bool ENABLE_SERIAL_TEST_TAG = TEST_MODE; // 't' in Serial Monitor
constexpr uint32_t DISPLAY_MIN_UPDATE_MS = 120;

// First row (y=0) runs left-to-right from the TOP-LEFT DIN corner.
// Each successive physical row reverses direction (serpentine wiring).
constexpr bool FIRST_ROW_LEFT_TO_RIGHT = true;
}
