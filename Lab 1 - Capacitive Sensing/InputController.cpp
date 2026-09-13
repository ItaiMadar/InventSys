#include "InputController.h"

InputController::SwipeTouchButton::SwipeTouchButton(
    uint8_t pin,
    const SwipeTouchSettings& settings)
    : pin_(pin), settings_(settings) {}

bool InputController::SwipeTouchButton::pressed(uint32_t nowMs) {
  if (hasSampled_ && nowMs - lastSampleMs_ < settings_.sampleIntervalMs) {
    return false;
  }

  hasSampled_ = true;
  lastSampleMs_ = nowMs;
  samples_[writeIndex_] = touchRead(pin_);
  writeIndex_ = (writeIndex_ + 1) % WindowSize;

  if (sampleCount_ < WindowSize) {
    ++sampleCount_;
  }
  if (sampleCount_ < WindowSize) {
    return false;
  }

  const size_t oldest = writeIndex_;
  const size_t newest = (writeIndex_ + WindowSize - 1) % WindowSize;
  const float meanDerivative =
      static_cast<float>(samples_[newest] - samples_[oldest]) /
      static_cast<float>(WindowSize - 1);

  const bool thresholdNow = settings_.pressIsRising
                                ? meanDerivative > settings_.derivativeThreshold
                                : meanDerivative < -settings_.derivativeThreshold;

  if (thresholdNow && !thresholdActive_) {
    thresholdActive_ = true;
    return true;
  }
  if (!thresholdNow) {
    thresholdActive_ = false;
  }
  return false;
}

InputController::ModeTouchButton::ModeTouchButton(
    uint8_t pin,
    const ModeTouchSettings& settings)
    : pin_(pin), settings_(settings) {}

bool InputController::ModeTouchButton::pressed(uint32_t nowMs) {
  if (initialized_ && nowMs - lastSampleMs_ < settings_.sampleIntervalMs) {
    return false;
  }

  lastSampleMs_ = nowMs;
  const float rawValue = static_cast<float>(touchRead(pin_));

  if (!initialized_) {
    filteredValue_ = rawValue;
    baseline_ = rawValue;
    initialized_ = true;
    return false;
  }

  filteredValue_ += settings_.filterAlpha * (rawValue - filteredValue_);
  const float touchAmount = settings_.pressIsRising
                                ? filteredValue_ - baseline_
                                : baseline_ - filteredValue_;

  if (!held_) {
    if (touchAmount >= settings_.pressThreshold) {
      if (++transitionSamples_ >= settings_.debounceSamples) {
        held_ = true;
        transitionSamples_ = 0;
        return true;
      }
    } else {
      transitionSamples_ = 0;
      baseline_ += settings_.baselineAlpha * (filteredValue_ - baseline_);
    }
  } else if (touchAmount <= settings_.releaseThreshold) {
    if (++transitionSamples_ >= settings_.debounceSamples) {
      held_ = false;
      transitionSamples_ = 0;
    }
  } else {
    transitionSamples_ = 0;
  }

  return false;
}

InputController::SwipeTracker::SwipeTracker(uint32_t timeoutMs)
    : timeoutMs_(timeoutMs) {}

void InputController::SwipeTracker::reset() {
  count_ = 0;
}

void InputController::SwipeTracker::update(uint32_t nowMs) {
  if (count_ > 0 && nowMs - times_[0] > timeoutMs_) {
    reset();
  }
}

void InputController::SwipeTracker::startAtEndpoint(
    uint8_t sensorIndex,
    uint32_t nowMs) {
  if (sensorIndex == 0 || sensorIndex == 2) {
    sequence_[0] = sensorIndex;
    times_[0] = nowMs;
    count_ = 1;
  }
}

bool InputController::SwipeTracker::isExpected(uint8_t sensorIndex) const {
  if (count_ == 1) {
    return sensorIndex == 1;
  }
  if (count_ == 2) {
    return sequence_[0] == 0 ? sensorIndex == 2 : sensorIndex == 0;
  }
  return false;
}

InputEvent InputController::SwipeTracker::record(
    uint8_t sensorIndex,
    uint32_t nowMs) {
  InputEvent event;
  update(nowMs);

  if (sensorIndex > 2) {
    return event;
  }
  if (count_ == 0) {
    startAtEndpoint(sensorIndex, nowMs);
    return event;
  }
  if (sensorIndex == sequence_[count_ - 1]) {
    return event;
  }
  if (!isExpected(sensorIndex)) {
    reset();
    startAtEndpoint(sensorIndex, nowMs);
    return event;
  }

  sequence_[count_] = sensorIndex;
  times_[count_] = nowMs;
  ++count_;

  if (count_ < 3) {
    return event;
  }

  event.type = sequence_[0] == 0
                   ? InputEventType::SwipeForward
                   : InputEventType::SwipeReverse;
  event.firstIntervalMs = times_[1] - times_[0];
  event.secondIntervalMs = times_[2] - times_[1];
  reset();
  return event;
}

InputController::InputController(const Settings& settings)
    : settings_(settings),
      swipeButtons_{
          SwipeTouchButton(settings.swipePins[0], settings.swipeTouch),
          SwipeTouchButton(settings.swipePins[1], settings.swipeTouch),
          SwipeTouchButton(settings.swipePins[2], settings.swipeTouch),
      },
      modeButton_(settings.modePin, settings.modeTouch),
      swipeTracker_(settings.swipeTimeoutMs) {}

InputEvent InputController::update(uint32_t nowMs) {
  InputEvent event;
  bool swipePadActivated = false;

  for (uint8_t i = 0; i < 3; ++i) {
    if (swipeButtons_[i].pressed(nowMs)) {
      swipePadActivated = true;
      lastSwipeActivityMs_ = nowMs;

      const InputEvent swipe = swipeTracker_.record(i, nowMs);
      if (swipe.type != InputEventType::None) {
        event = swipe;
      }
    }
  }
  swipeTracker_.update(nowMs);

  const bool modePressed = modeButton_.pressed(nowMs);
  const bool modeChangeAllowed =
      !swipePadActivated &&
      !swipeTracker_.isInProgress() &&
      nowMs - lastSwipeActivityMs_ >= settings_.modeTouch.swipeGuardMs;

  if (modePressed) {
    modeGestureActive_ = modeChangeAllowed;
    modeGestureStartedMs_ = nowMs;
  }

  if (swipePadActivated || swipeTracker_.isInProgress() ||
      !modeButton_.isHeld()) {
    modeGestureActive_ = false;
  }

  if (modeGestureActive_ &&
      nowMs - modeGestureStartedMs_ >= settings_.modeTouch.holdMs) {
    modeGestureActive_ = false;
    if (event.type == InputEventType::None) {
      event.type = InputEventType::ToggleMode;
    }
  }

  return event;
}
