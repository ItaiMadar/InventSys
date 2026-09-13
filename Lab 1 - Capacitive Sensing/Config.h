#pragma once

#include <Arduino.h>

#include "InputController.h"
#include "ParameterController.h"

namespace Config {

constexpr size_t OscillatorCount = 50; // Above 50 performance drops significantly
constexpr size_t LEDCount = 5;
constexpr uint8_t LedPins[LEDCount] = {15, 2, 4, 16, 17};

constexpr InputController::Settings Input = {
    {27, 33, 32},  // Swipe electrodes, ordered 1 -> 2 -> 3.
    13,            // Mode-toggle electrode.
    600,           // Swipe timeout, ms.
    {
        false,   // Swipe touch produces a rising signal.
        50.0f, // Mean-derivative threshold.
        10,     // Sampling interval, ms.
    },
    {
        true,    // Mode touch produces a rising signal.
        50.0f,  // Press threshold.
        15.0f,  // Release threshold.
        0.25f,  // Measurement filter alpha.
        0.002f, // Idle-baseline filter alpha.
        1,       // Consecutive samples required for press/release.
        5,      // Sampling interval, ms.
        10,      // Hold duration required to toggle mode, ms.
        50,      // Mode suppression after swipe activity, ms.
    },
};

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
