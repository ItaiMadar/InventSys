#pragma once

#include <Arduino.h>
#include <math.h>

enum class ControlMode {
  Frequency,
  Coupling,
};

class ParameterController {
 public:
  struct ParameterSettings {
    float initial;
    float minimum;
    float maximum;
    float swipeGain;
  };

  struct Settings {
    ParameterSettings frequency;
    ParameterSettings coupling;
  };

  explicit ParameterController(const Settings& settings)
      : settings_(settings),
        frequencyHz_(settings.frequency.initial),
        coupling_(settings.coupling.initial) {}

  void toggleMode() {
    mode_ = mode_ == ControlMode::Frequency
                ? ControlMode::Coupling
                : ControlMode::Frequency;
    lastChange_ = 0.0f;
  }

  bool applySwipe(int direction, uint32_t durationMs) {
    if ((direction != 1 && direction != -1) || durationMs == 0) {
      return false;
    }

    const float durationSeconds = static_cast<float>(durationMs) / 1000.0f;
    const float speedFactor = 1.0f / (1.0f + sqrtf(durationSeconds));
    const float sign = static_cast<float>(direction);

    if (mode_ == ControlMode::Frequency) {
      lastChange_ = sign * settings_.frequency.swipeGain * speedFactor;
      frequencyHz_ = constrain(frequencyHz_ + lastChange_,
                               settings_.frequency.minimum,
                               settings_.frequency.maximum);
    } else {
      lastChange_ = sign * settings_.coupling.swipeGain * speedFactor;
      coupling_ = constrain(coupling_ + lastChange_,
                            settings_.coupling.minimum,
                            settings_.coupling.maximum);
    }
    return true;
  }

  ControlMode mode() const { return mode_; }
  float frequencyHz() const { return frequencyHz_; }
  float coupling() const { return coupling_; }
  float lastChange() const { return lastChange_; }

 private:
  Settings settings_;
  ControlMode mode_ = ControlMode::Frequency;
  float frequencyHz_;
  float coupling_;
  float lastChange_ = 0.0f;
};
