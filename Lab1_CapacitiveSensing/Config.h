#pragma once

#include <Arduino.h>

#include "ParameterController.h"

namespace Config {

constexpr size_t OscillatorCount = 50; // Above 50 performance drops significantly
constexpr size_t LEDCount = 5;
constexpr uint8_t LedPins[LEDCount] = {15, 2, 4, 16, 17};

constexpr uint8_t SwipeTouchPins[3] = {27, 33, 32};
constexpr uint8_t ModeTouchPin = 13;
constexpr float TouchDerivativeThreshold = 20.0f;
constexpr uint32_t SwipeTimeoutMs = 750;
constexpr uint32_t LoopDelayMs = 10;

constexpr ParameterController::Settings Parameters = {
    {
        1.0f, // Initial frequency, Hz.
        0.5f, // Minimum frequency, Hz.
        3.0f, // Maximum frequency, Hz.
        0.5f, // Frequency swipe gain.
    },
    {
        0.0f, // Initial K, rad/s.
        0.0f, // Minimum K, rad/s.
        10.0f, // Maximum K, rad/s.
        1.0f, // Coupling swipe gain.
    },
};

constexpr uint32_t SerialBaud = 115200;
constexpr uint32_t DiagnosticsIntervalMs = 200;

inline float randomFloat(float minimum, float maximum) {
  const float normalized = random(0, 1000000) / 1000000.0f;
  return minimum + normalized * (maximum - minimum);
}

}  // namespace Config
