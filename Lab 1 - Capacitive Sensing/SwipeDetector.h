#pragma once

#include <Arduino.h>

enum class SwipeDirection : int8_t {
  None = 0,
  Forward = 1,  // sensor 1 -> 2 -> 3
  Reverse = -1 // sensor 3 -> 2 -> 1
};

struct SwipeEvent {
  bool detected = false;
  SwipeDirection direction = SwipeDirection::None;
  uint32_t delta1Ms = 0;
  uint32_t delta2Ms = 0;

  uint32_t durationMs() const { return delta1Ms + delta2Ms; }
};

class SwipeDetector {
 public:
  explicit SwipeDetector(uint32_t timeoutMs);

  SwipeEvent recordTouch(uint8_t sensorIndex, uint32_t nowMs);
  void update(uint32_t nowMs);
  void reset();
  bool isInProgress() const { return count_ > 0; }

 private:
  uint32_t timeoutMs_;
  uint8_t sequence_[3] = {};
  uint32_t times_[3] = {};
  uint8_t count_ = 0;

  bool isExpectedNext(uint8_t sensorIndex) const;
  void startIfEndpoint(uint8_t sensorIndex, uint32_t nowMs);
};
