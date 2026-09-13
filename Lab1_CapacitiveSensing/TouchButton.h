#pragma once

#include <Arduino.h>

class TouchButton {
 public:
  TouchButton(uint8_t pin, float derivativeThreshold)
      : pin_(pin), derivativeThreshold_(derivativeThreshold) {}

  bool pressed() {
    const int sensedValue = touchRead(pin_);

    measurements_[bufferIndex_] = sensedValue;
    bufferIndex_ = (bufferIndex_ + 1) % MeasurementCount;

    if (sampleCount_ < MeasurementCount) {
      ++sampleCount_;
    }
    if (sampleCount_ < MeasurementCount) {
      return false;
    }

    float derivativeSum = 0.0f;
    for (size_t i = 0; i < MeasurementCount - 1; ++i) {
      const size_t first = (bufferIndex_ + i) % MeasurementCount;
      const size_t second = (bufferIndex_ + i + 1) % MeasurementCount;
      derivativeSum += measurements_[second] - measurements_[first];
    }

    const float meanDerivative =
        derivativeSum / static_cast<float>(MeasurementCount - 1);
    const bool thresholdExceeded = meanDerivative > derivativeThreshold_;
    const bool newPress = thresholdExceeded && !wasAboveThreshold_;
    wasAboveThreshold_ = thresholdExceeded;
    return newPress;
  }

 private:
  static constexpr size_t MeasurementCount = 10;

  uint8_t pin_;
  float derivativeThreshold_;
  int measurements_[MeasurementCount] = {};
  size_t bufferIndex_ = 0;
  size_t sampleCount_ = 0;
  bool wasAboveThreshold_ = false;
};
