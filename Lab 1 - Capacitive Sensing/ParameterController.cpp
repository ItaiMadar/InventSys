#include "ParameterController.h"

#include <Arduino.h>

ParameterController::ParameterController(float initialFrequencyHz,
                                         float minFrequencyHz,
                                         float maxFrequencyHz,
                                         float frequencySwipeGain,
                                         float initialCoupling,
                                         float minCoupling,
                                         float maxCoupling,
                                         float couplingSwipeGain)
    : minFrequencyHz_(minFrequencyHz),
      maxFrequencyHz_(maxFrequencyHz),
      frequencySwipeGain_(frequencySwipeGain),
      minCoupling_(minCoupling),
      maxCoupling_(maxCoupling),
      couplingSwipeGain_(couplingSwipeGain),
      frequencyHz_(initialFrequencyHz),
      coupling_(initialCoupling) {}

void ParameterController::toggleMode() {
  mode_ = mode_ == ControlMode::Frequency
              ? ControlMode::Coupling
              : ControlMode::Frequency;
}

void ParameterController::apply(const SwipeEvent& swipe) {
  if (!swipe.detected || swipe.durationMs() == 0) {
    return;
  }
  float swipeDuration = static_cast<float>(swipe.durationMs()) / 1000.0f;
  const float inverseDuration = 1 / (1 + sqrt(swipeDuration));
  const float sign =
      swipe.direction == SwipeDirection::Forward ? 1.0f : -1.0f;

  if (mode_ == ControlMode::Frequency) {
    lastChange_ = sign * frequencySwipeGain_ * inverseDuration;
    frequencyHz_ = constrain(frequencyHz_ + lastChange_,
                             minFrequencyHz_,
                             maxFrequencyHz_);
  } else {
    lastChange_ = sign * couplingSwipeGain_ * inverseDuration;
    coupling_ = constrain(coupling_ + lastChange_,
                          minCoupling_,
                          maxCoupling_);
  }
}
