#pragma once

#include <Arduino.h>

class TouchButton {
 public:
  enum class Edge { Rising, Falling };

  TouchButton(uint8_t pin,
              float derivativeThreshold,
              Edge pressEdge,
              uint32_t sampleIntervalMs);

  // Call frequently. Returns true once when a new press edge is detected.
  bool pressed(uint32_t nowMs);

  int rawValue() const { return rawValue_; }
  float meanDerivative() const { return meanDerivative_; }

 private:
  static constexpr size_t WindowSize = 10;

  uint8_t pin_;
  float threshold_;
  Edge pressEdge_;
  uint32_t sampleIntervalMs_;

  int samples_[WindowSize] = {};
  size_t writeIndex_ = 0;
  size_t sampleCount_ = 0;
  uint32_t lastSampleMs_ = 0;
  bool hasSampled_ = false;
  bool thresholdExceeded_ = false;

  int rawValue_ = 0;
  float meanDerivative_ = 0.0f;
};
