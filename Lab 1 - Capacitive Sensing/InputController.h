#pragma once

#include <Arduino.h>

enum class InputEventType {
  None,
  ToggleMode,
  SwipeForward,
  SwipeReverse,
};

struct InputEvent {
  InputEventType type = InputEventType::None;
  uint32_t firstIntervalMs = 0;
  uint32_t secondIntervalMs = 0;

  uint32_t durationMs() const {
    return firstIntervalMs + secondIntervalMs;
  }
};

class InputController {
 public:
  struct SwipeTouchSettings {
    bool pressIsRising;
    float derivativeThreshold;
    uint32_t sampleIntervalMs;
  };

  struct ModeTouchSettings {
    bool pressIsRising;
    float pressThreshold;
    float releaseThreshold;
    float filterAlpha;
    float baselineAlpha;
    uint8_t debounceSamples;
    uint32_t sampleIntervalMs;
    uint32_t holdMs;
    uint32_t swipeGuardMs;
  };

  struct Settings {
    uint8_t swipePins[3];
    uint8_t modePin;
    uint32_t swipeTimeoutMs;
    SwipeTouchSettings swipeTouch;
    ModeTouchSettings modeTouch;
  };

  explicit InputController(const Settings& settings);

  // Polls all four electrodes and emits at most one high-level input event.
  InputEvent update(uint32_t nowMs);

 private:
  class SwipeTouchButton {
   public:
    SwipeTouchButton(uint8_t pin, const SwipeTouchSettings& settings);
    bool pressed(uint32_t nowMs);

   private:
    static constexpr size_t WindowSize = 10;

    uint8_t pin_;
    SwipeTouchSettings settings_;
    int samples_[WindowSize] = {};
    size_t writeIndex_ = 0;
    size_t sampleCount_ = 0;
    uint32_t lastSampleMs_ = 0;
    bool hasSampled_ = false;
    bool thresholdActive_ = false;
  };

  class ModeTouchButton {
   public:
    ModeTouchButton(uint8_t pin, const ModeTouchSettings& settings);
    bool pressed(uint32_t nowMs);
    bool isHeld() const { return held_; }

   private:
    uint8_t pin_;
    ModeTouchSettings settings_;
    uint32_t lastSampleMs_ = 0;
    bool initialized_ = false;
    bool held_ = false;
    uint8_t transitionSamples_ = 0;
    float filteredValue_ = 0.0f;
    float baseline_ = 0.0f;
  };

  class SwipeTracker {
   public:
    explicit SwipeTracker(uint32_t timeoutMs);
    InputEvent record(uint8_t sensorIndex, uint32_t nowMs);
    void update(uint32_t nowMs);
    bool isInProgress() const { return count_ > 0; }

   private:
    uint32_t timeoutMs_;
    uint8_t sequence_[3] = {};
    uint32_t times_[3] = {};
    uint8_t count_ = 0;

    void reset();
    void startAtEndpoint(uint8_t sensorIndex, uint32_t nowMs);
    bool isExpected(uint8_t sensorIndex) const;
  };

  Settings settings_;
  SwipeTouchButton swipeButtons_[3];
  ModeTouchButton modeButton_;
  SwipeTracker swipeTracker_;

  uint32_t lastSwipeActivityMs_ = 0;
  uint32_t modeGestureStartedMs_ = 0;
  bool modeGestureActive_ = false;
};
