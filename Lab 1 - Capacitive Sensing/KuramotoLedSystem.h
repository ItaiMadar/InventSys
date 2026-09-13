#pragma once

#include <Arduino.h>

class KuramotoLedSystem {
 public:
  static constexpr size_t Count = 5;

  KuramotoLedSystem(const uint8_t (&ledPins)[Count],
                    const float (&frequencyOffsetsHz)[Count]);

  void begin();
  void setBaseFrequency(float frequencyHz) { baseFrequencyHz_ = frequencyHz; }
  void setCoupling(float coupling) { coupling_ = coupling; }
  void update(uint32_t nowMicros);

  float orderParameter() const;

 private:
  static constexpr float IntegrationStepSeconds = 0.005f;

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
