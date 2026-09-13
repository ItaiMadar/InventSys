#include "SwipeDetector.h"

SwipeDetector::SwipeDetector(uint32_t timeoutMs) : timeoutMs_(timeoutMs) {}

void SwipeDetector::reset() {
  count_ = 0;
}

void SwipeDetector::update(uint32_t nowMs) {
  if (count_ > 0 && nowMs - times_[0] > timeoutMs_) {
    reset();
  }
}

void SwipeDetector::startIfEndpoint(uint8_t sensorIndex, uint32_t nowMs) {
  // A valid swipe can start only at the left or right endpoint.
  if (sensorIndex == 0 || sensorIndex == 2) {
    sequence_[0] = sensorIndex;
    times_[0] = nowMs;
    count_ = 1;
  }
}

bool SwipeDetector::isExpectedNext(uint8_t sensorIndex) const {
  if (count_ == 1) {
    return sensorIndex == 1;
  }
  if (count_ == 2) {
    return sequence_[0] == 0 ? sensorIndex == 2 : sensorIndex == 0;
  }
  return false;
}

SwipeEvent SwipeDetector::recordTouch(uint8_t sensorIndex, uint32_t nowMs) {
  SwipeEvent event;
  update(nowMs);

  if (sensorIndex > 2) {
    return event;
  }

  if (count_ == 0) {
    startIfEndpoint(sensorIndex, nowMs);
    return event;
  }

  // Ignore duplicate activation of the most recent electrode.
  if (sensorIndex == sequence_[count_ - 1]) {
    return event;
  }

  if (!isExpectedNext(sensorIndex)) {
    reset();
    startIfEndpoint(sensorIndex, nowMs);
    return event;
  }

  sequence_[count_] = sensorIndex;
  times_[count_] = nowMs;
  ++count_;

  if (count_ < 3) {
    return event;
  }

  event.detected = true;
  event.direction = sequence_[0] == 0
                        ? SwipeDirection::Forward
                        : SwipeDirection::Reverse;
  event.delta1Ms = times_[1] - times_[0];
  event.delta2Ms = times_[2] - times_[1];
  reset();
  return event;
}
