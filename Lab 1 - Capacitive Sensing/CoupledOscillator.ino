#include "Config.h"
#include "KuramotoLedSystem.h"
#include "ModeButton.h"
#include "ParameterController.h"
#include "SwipeDetector.h"
#include "TouchButton.h"

const TouchButton::Edge PressEdge =
    Config::TouchPressIsRising
        ? TouchButton::Edge::Rising
        : TouchButton::Edge::Falling;

TouchButton swipeButtons[Config::SwipeSensorCount] = {
    TouchButton(Config::SwipeTouchPins[0],
                Config::TouchDerivativeThreshold,
                PressEdge,
                Config::TouchSampleIntervalMs),
    TouchButton(Config::SwipeTouchPins[1],
                Config::TouchDerivativeThreshold,
                PressEdge,
                Config::TouchSampleIntervalMs),
    TouchButton(Config::SwipeTouchPins[2],
                Config::TouchDerivativeThreshold,
                PressEdge,
                Config::TouchSampleIntervalMs),
};

SwipeDetector swipeDetector(Config::SwipeTimeoutMs);

ModeButton modeButton(Config::ModeButtonPin,
                      Config::ModePressIsRising,
                      Config::ModePressThreshold,
                      Config::ModeReleaseThreshold,
                      Config::ModeFilterAlpha,
                      Config::ModeBaselineAlpha,
                      Config::ModeDebounceSamples,
                      Config::ModeSampleIntervalMs);

ParameterController parameters(Config::BaseFrequencyHz,
                               Config::MinFrequencyHz,
                               Config::MaxFrequencyHz,
                               Config::FrequencySwipeGain,
                               Config::InitialCoupling,
                               Config::MinCoupling,
                               Config::MaxCoupling,
                               Config::CouplingSwipeGain);

KuramotoLedSystem oscillators(Config::LedPins,
                              Config::FrequencyOffsetsHz);

uint32_t lastDiagnosticsMs = 0;
uint32_t lastSwipeActivityMs = 0;
uint32_t modeGestureStartedMs = 0;
bool modeGestureActive = false;

const char* modeName() {
  return parameters.mode() == ControlMode::Frequency ? "frequency" : "K";
}

void handleSwipe(const SwipeEvent& swipe) {
  if (!swipe.detected) {
    return;
  }

  parameters.apply(swipe);
  oscillators.setBaseFrequency(parameters.frequencyHz());
  oscillators.setCoupling(parameters.coupling());

  // Serial.print("swipe=");
  // Serial.print(swipe.direction == SwipeDirection::Forward ? "forward" : "reverse");
  // Serial.print(", duration_ms=");
  // Serial.print(swipe.durationMs());
  // Serial.print(", mode=");
  // Serial.print(modeName());
  // Serial.print(", change=");
  // Serial.print(parameters.lastChange(), 3);
  // Serial.print(", base_f_hz=");
  // Serial.print(parameters.frequencyHz(), 3);
  // Serial.print(", K=");
  // Serial.println(parameters.coupling(), 3);
}

void printDiagnostics(uint32_t nowMs) {
  if (nowMs - lastDiagnosticsMs < Config::DiagnosticsIntervalMs) {
    return;
  }
  lastDiagnosticsMs = nowMs;
  Serial.print("RelativeCoupling:");
  Serial.print(parameters.coupling() / Config::MaxCoupling, 3);
  Serial.print(", RelativeFrequency:");
  Serial.print(parameters.frequencyHz() / Config::MaxFrequencyHz, 3);
  Serial.print(", OrderParameter:");
  Serial.println(oscillators.orderParameter(), 3);
}

void setup() {
  Serial.begin(Config::SerialBaud);
  oscillators.begin();
  oscillators.setBaseFrequency(parameters.frequencyHz());
  oscillators.setCoupling(parameters.coupling());
  Serial.println(" Control mode: frequency");
}

void loop() {
  const uint32_t nowMs = millis();

  bool swipePadActivated = false;
  for (uint8_t i = 0; i < Config::SwipeSensorCount; ++i) {
    if (swipeButtons[i].pressed(nowMs)) {
      swipePadActivated = true;
      lastSwipeActivityMs = nowMs;
      handleSwipe(swipeDetector.recordTouch(i, nowMs));
    }
  }
  swipeDetector.update(nowMs);

  // A mode change is accepted only as an isolated button press. This prevents
  // capacitive cross-talk from a swipe from changing the selected parameter.
  const bool modePressed = modeButton.pressed(nowMs);
  const bool modeChangeAllowed =
      !swipePadActivated &&
      !swipeDetector.isInProgress() &&
      nowMs - lastSwipeActivityMs >= Config::ModeSwipeGuardMs;

  if (modePressed) {
    modeGestureActive = modeChangeAllowed;
    modeGestureStartedMs = nowMs;
  }

  // A swipe that begins during the confirmation interval cancels the mode
  // gesture. An ignored press cannot become valid later while still held.
  if (swipePadActivated || swipeDetector.isInProgress()) {
    modeGestureActive = false;
  }

  if (!modeButton.isHeld()) {
    modeGestureActive = false;
  }

  if (modeGestureActive &&
      nowMs - modeGestureStartedMs >= Config::ModeHoldMs) {
    parameters.toggleMode();
    modeGestureActive = false;
    Serial.print("Control mode: ");
    Serial.println(modeName());
  }

  oscillators.update(micros());

  printDiagnostics(nowMs);
}
