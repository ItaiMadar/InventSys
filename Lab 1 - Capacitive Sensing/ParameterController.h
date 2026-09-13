#pragma once

#include "SwipeDetector.h"

enum class ControlMode {
  Frequency,
  Coupling
};

class ParameterController {
 public:
  ParameterController(float initialFrequencyHz,
                      float minFrequencyHz,
                      float maxFrequencyHz,
                      float frequencySwipeGain,
                      float initialCoupling,
                      float minCoupling,
                      float maxCoupling,
                      float couplingSwipeGain);

  void toggleMode();
  void apply(const SwipeEvent& swipe);

  ControlMode mode() const { return mode_; }
  float frequencyHz() const { return frequencyHz_; }
  float coupling() const { return coupling_; }
  float lastChange() const { return lastChange_; }

 private:
  ControlMode mode_ = ControlMode::Frequency;
  float minFrequencyHz_;
  float maxFrequencyHz_;
  float frequencySwipeGain_;
  float minCoupling_;
  float maxCoupling_;
  float couplingSwipeGain_;
  float frequencyHz_;
  float coupling_;
  float lastChange_ = 0.0f;
};
