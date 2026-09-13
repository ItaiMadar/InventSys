#include "TouchButton.h"

TouchButton::TouchButton(uint8_t pin,
                         float derivativeThreshold,
                         Edge pressEdge,
                         uint32_t sampleIntervalMs)
    : pin_(pin),
      threshold_(derivativeThreshold),
      pressEdge_(pressEdge),
      sampleIntervalMs_(sampleIntervalMs) {}

bool TouchButton::pressed(uint32_t nowMs) {
  if (hasSampled_ && nowMs - lastSampleMs_ < sampleIntervalMs_) {
    return false;
  }

  hasSampled_ = true;
  lastSampleMs_ = nowMs;
  rawValue_ = touchRead(pin_);

  samples_[writeIndex_] = rawValue_;
  writeIndex_ = (writeIndex_ + 1) % WindowSize;

  if (sampleCount_ < WindowSize) {
    ++sampleCount_;
  }
  if (sampleCount_ < WindowSize) {
    return false;
  }

  // Averaging the adjacent differences is equivalent to
  // (newest sample - oldest sample) / (window size - 1).
  const size_t oldestIndex = writeIndex_;
  const size_t newestIndex =
      (writeIndex_ + WindowSize - 1) % WindowSize;
  meanDerivative_ =
      static_cast<float>(samples_[newestIndex] - samples_[oldestIndex]) /
      static_cast<float>(WindowSize - 1);

  const bool isAboveThreshold =
      pressEdge_ == Edge::Rising
          ? meanDerivative_ > threshold_
          : meanDerivative_ < -threshold_;

  if (isAboveThreshold && !thresholdExceeded_) {
    thresholdExceeded_ = true;
    return true;
  }

  if (!isAboveThreshold) {
    thresholdExceeded_ = false;
  }
  return false;
}
