#pragma once

#include <Arduino.h>

class TouchButton {
 public:
  TouchButton(uint8_t pin, float derivativeThreshold)
      : pin_(pin), derivativeThreshold_(derivativeThreshold) {}

  bool pressed() {
    const int sensedValue = touchRead(pin_); // Read value

    // Running window of length {MeasurementCount} from which we calculate the mean derivative value
    measurements_[bufferIndex_] = sensedValue; 
    bufferIndex_ = (bufferIndex_ + 1) % MeasurementCount;

    // Do not go through logic with less than {MeasurementCount} reads
    if (sampleCount_ < MeasurementCount) {
      ++sampleCount_;
      return false;
    }

    // Calculate mean derivative
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
    return newPress; // Returns true only if the previous iteration did not exceed derivative threshold
  }

// Init. parameters
 private:
  static constexpr size_t MeasurementCount = 10;

  uint8_t pin_;
  float derivativeThreshold_;
  int measurements_[MeasurementCount] = {};
  size_t bufferIndex_ = 0;
  size_t sampleCount_ = 0;
  bool wasAboveThreshold_ = false;
};
