#pragma once

#include <Arduino.h>

class ModeButton {
 public:
  ModeButton(uint8_t pin,
             bool pressIsRising,
             float pressThreshold,
             float releaseThreshold,
             float filterAlpha,
             float baselineAlpha,
             uint8_t debounceSamples,
             uint32_t sampleIntervalMs);

  // Returns true once per complete physical press. A new event cannot occur
  // until a stable release has been observed.
  bool pressed(uint32_t nowMs);

  int rawValue() const { return rawValue_; }
  float touchAmount() const { return touchAmount_; }
  bool isHeld() const { return held_; }

 private:
  uint8_t pin_;
  bool pressIsRising_;
  float pressThreshold_;
  float releaseThreshold_;
  float filterAlpha_;
  float baselineAlpha_;
  uint8_t debounceSamples_;
  uint32_t sampleIntervalMs_;

  uint32_t lastSampleMs_ = 0;
  bool initialized_ = false;
  bool held_ = false;
  uint8_t transitionSamples_ = 0;
  int rawValue_ = 0;
  float filteredValue_ = 0.0f;
  float baseline_ = 0.0f;
  float touchAmount_ = 0.0f;
};
