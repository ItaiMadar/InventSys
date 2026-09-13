#pragma once

#include "Config.h"
#include <Arduino.h>

const size_t LEDCount = Config::LEDCount;
const size_t Count = Config::OscillatorCount;

class KuramotoLedSystem {
 public:
  KuramotoLedSystem(const uint8_t (&ledPins)[LEDCount],
                    const float (&frequencyOffsetsHz)[Count]);

  void begin();
  void setBaseFrequency(float frequencyHz) { baseFrequencyHz_ = frequencyHz; }
  void setCoupling(float coupling) { coupling_ = coupling; }
  void update(uint32_t nowMicros);
  float orderParameter() const;

 private:
  static constexpr float IntegrationStepSeconds = 0.005f;
  static constexpr float FlashSharpness = 8.0f;

  uint8_t ledPins_[Count];
  float offsetsHz_[Count];
  float phases_[Count];
  float baseFrequencyHz_ = 1.0f;
  float coupling_ = 0.0f;
  uint32_t previousMicros_ = 0;
  float accumulatedSeconds_ = 0.0f;

  void integrate(float dt);
  void render() const;
};
