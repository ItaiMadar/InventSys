#include "ModeButton.h"

ModeButton::ModeButton(uint8_t pin,
                       bool pressIsRising,
                       float pressThreshold,
                       float releaseThreshold,
                       float filterAlpha,
                       float baselineAlpha,
                       uint8_t debounceSamples,
                       uint32_t sampleIntervalMs)
    : pin_(pin),
      pressIsRising_(pressIsRising),
      pressThreshold_(pressThreshold),
      releaseThreshold_(releaseThreshold),
      filterAlpha_(filterAlpha),
      baselineAlpha_(baselineAlpha),
      debounceSamples_(debounceSamples),
      sampleIntervalMs_(sampleIntervalMs) {}

bool ModeButton::pressed(uint32_t nowMs) {
  if (initialized_ && nowMs - lastSampleMs_ < sampleIntervalMs_) {
    return false;
  }

  lastSampleMs_ = nowMs;
  rawValue_ = touchRead(pin_);

  if (!initialized_) {
    filteredValue_ = static_cast<float>(rawValue_);
    baseline_ = filteredValue_;
    initialized_ = true;
    return false;
  }

  filteredValue_ +=
      filterAlpha_ * (static_cast<float>(rawValue_) - filteredValue_);
  touchAmount_ = pressIsRising_
                     ? filteredValue_ - baseline_
                     : baseline_ - filteredValue_;

  if (!held_) {
    if (touchAmount_ >= pressThreshold_) {
      if (++transitionSamples_ >= debounceSamples_) {
        held_ = true;
        transitionSamples_ = 0;
        return true;
      }
    } else {
      transitionSamples_ = 0;
      baseline_ += baselineAlpha_ * (filteredValue_ - baseline_);
    }
  } else {
    if (touchAmount_ <= releaseThreshold_) {
      if (++transitionSamples_ >= debounceSamples_) {
        held_ = false;
        transitionSamples_ = 0;
      }
    } else {
      transitionSamples_ = 0;
    }
  }

  return false;
}
