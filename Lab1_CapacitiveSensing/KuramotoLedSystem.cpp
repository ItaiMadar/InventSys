#include "KuramotoLedSystem.h"

#include <math.h>

KuramotoLedSystem::KuramotoLedSystem(
    const uint8_t (&ledPins)[LEDCount],
    const float (&frequencyOffsetsHz)[Count]) {
  for (size_t i = 0; i < Count; ++i) {
    offsetsHz_[i] = Config::randomFloat(-0.8f, 0.8f);
    phases_[i] = TWO_PI * static_cast<float>(i) / static_cast<float>(Count);
    if (i < LEDCount){
      ledPins_[i] = ledPins[i];
    }
  }
}

void KuramotoLedSystem::begin() {
  for (size_t i = 0; i < LEDCount; ++i) {
    pinMode(ledPins_[i], OUTPUT);
    analogWrite(ledPins_[i], 0);
  }
}

void KuramotoLedSystem::update(uint32_t nowMicros) {
  if (previousMicros_ == 0) {
    previousMicros_ = nowMicros;
    render();
    return;
  }

  float elapsedSeconds =
      static_cast<float>(nowMicros - previousMicros_) / 1000000.0f;
  previousMicros_ = nowMicros;
  elapsedSeconds = constrain(elapsedSeconds, 0.0f, 0.1f);
  accumulatedSeconds_ += elapsedSeconds;

  while (accumulatedSeconds_ >= IntegrationStepSeconds) {
    integrate(IntegrationStepSeconds);
    accumulatedSeconds_ -= IntegrationStepSeconds;
  }
  render();
}

void KuramotoLedSystem::integrate(float dt) {
  float phaseRates[Count];

  for (size_t i = 0; i < Count; ++i) {
    float couplingSum = 0.0f;
    for (size_t j = 0; j < Count; ++j) {
      couplingSum += sinf(phases_[j] - phases_[i]);
    }

    const float naturalFrequencyHz = baseFrequencyHz_ + offsetsHz_[i];
    const float omega = TWO_PI * max(naturalFrequencyHz, 0.01f);
    phaseRates[i] =
        omega + (coupling_ / static_cast<float>(Count)) * couplingSum;
  }

  for (size_t i = 0; i < Count; ++i) {
    phases_[i] = fmodf(phases_[i] + phaseRates[i] * dt, TWO_PI);
    if (phases_[i] < 0.0f) {
      phases_[i] += TWO_PI;
    }
  }
}

void KuramotoLedSystem::render() const {
  for (size_t i = 0; i < LEDCount; ++i) {
    float intensity = 0.5f * (sinf(phases_[i]) + 1.0f);
    intensity = powf(intensity, FlashSharpness);
    const int brightness = static_cast<int>(255.0f * intensity + 0.5f);
    analogWrite(ledPins_[i], brightness);
  }
}

float KuramotoLedSystem::orderParameter() const {
  float realPart = 0.0f;
  float imaginaryPart = 0.0f;

  for (size_t i = 0; i < Count; ++i) {
    realPart += cosf(phases_[i]);
    imaginaryPart += sinf(phases_[i]);
  }

  realPart /= static_cast<float>(Count);
  imaginaryPart /= static_cast<float>(Count);
  return sqrtf(realPart * realPart + imaginaryPart * imaginaryPart);
}
