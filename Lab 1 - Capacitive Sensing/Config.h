#pragma once

#include <Arduino.h>

// Function to calculate a random float between a min and max value
float getRandomFloat(float minVal, float maxVal) {
  // Generates a normalized decimal between 0.0 and 1.0
  float normalized = random(0, 1000000) / 1000000.0;
  
  // Scales the decimal to your custom range
  return minVal + normalized * (maxVal - minVal);
}

namespace Config {

// Three capacitive electrodes used for swipe detection.
constexpr uint8_t SwipeTouchPins[] = {27, 33, 32};
constexpr size_t SwipeSensorCount =
    sizeof(SwipeTouchPins) / sizeof(SwipeTouchPins[0]);

// A separate capacitive button toggles whether swipes tune frequency or K.
// GPIO 13 is touch-capable on the original ESP32. Change this for your board.
constexpr uint8_t ModeButtonPin = 13;

// Five PWM-capable LED outputs, ordered from left to right.
constexpr uint8_t LedPins[] = {15, 2, 4, 16, 17};
constexpr size_t OscillatorCount =
    sizeof(LedPins) / sizeof(LedPins[0]);

static_assert(SwipeSensorCount == 3,
              "SwipeDetector requires exactly three touch sensors");
static_assert(OscillatorCount == 5,
              "This demonstration is configured for five oscillators");

// Touch-button detector. The original sketch detected a rising mean derivative.
// If touching your electrode makes touchRead() fall, change this to false.
constexpr bool TouchPressIsRising = true;
constexpr float TouchDerivativeThreshold = 50.0f;
constexpr uint32_t TouchSampleIntervalMs = 10;

// A swipe must be completed before this timeout.
constexpr uint32_t SwipeTimeoutMs = 600;

// Base-frequency controller, in Hz.
constexpr float BaseFrequencyHz = 1.0f;
constexpr float MinFrequencyHz = 0.50f;
constexpr float MaxFrequencyHz = 3.0f;
constexpr float FrequencySwipeGain = 0.50f;

// // Natural-frequency offsets make synchronization non-trivial.
// constexpr float FrequencyOffsetsHz[OscillatorCount] = {
//     -0.16f, -0.08f, 0.0f, 0.08f, 0.16f
// };

// Natural-frequency offsets make synchronization non-trivial.
 float FrequencyOffsetsHz[OscillatorCount] = {
    getRandomFloat(-0.2, 0.2), getRandomFloat(-0.2, 0.2), 0.0f, getRandomFloat(-0.2, 0.2), getRandomFloat(-0.2, 0.2)
};

// K is angular-frequency coupling, measured in radians per second.
constexpr float InitialCoupling = 0.0f;
constexpr float MinCoupling = 0.0f;
constexpr float MaxCoupling = 5.0f;
constexpr float CouplingSwipeGain = 0.50f;

// Mode-button detection. This baseline-relative detector has hysteresis and
// requires several consecutive samples, unlike the derivative swipe detector.
constexpr bool ModePressIsRising = true;
constexpr float ModePressThreshold = 50.0f;
constexpr float ModeReleaseThreshold = 15.0f;
constexpr float ModeFilterAlpha = 0.25f;
constexpr float ModeBaselineAlpha = 0.002f;
constexpr uint8_t ModeDebounceSamples = 3;
constexpr uint32_t ModeSampleIntervalMs = 10;
constexpr uint32_t ModeHoldMs = 50;
constexpr uint32_t ModeSwipeGuardMs = 50;

constexpr uint32_t SerialBaud = 115200;
constexpr uint32_t DiagnosticsIntervalMs = 200;

}  // namespace Config
