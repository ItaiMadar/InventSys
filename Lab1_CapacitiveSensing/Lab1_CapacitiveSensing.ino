#include "Config.h"
#include "KuramotoLedSystem.h"
#include "ParameterController.h"
#include "TouchButton.h"

float frequencyOffsets[Config::OscillatorCount];

ParameterController parameters(Config::Parameters);
KuramotoLedSystem oscillators(Config::LedPins, frequencyOffsets);

TouchButton swipeButton1(Config::SwipeTouchPins[0],
                         Config::TouchDerivativeThreshold);
TouchButton swipeButton2(Config::SwipeTouchPins[1],
                         Config::TouchDerivativeThreshold);
TouchButton swipeButton3(Config::SwipeTouchPins[2],
                         Config::TouchDerivativeThreshold);
TouchButton modeButton(Config::ModeTouchPin,
                       Config::TouchDerivativeThreshold);

uint32_t touchTime1 = 0;
uint32_t touchTime2 = 0;
uint32_t touchTime3 = 0;
uint32_t lastDiagnosticsMs = 0;

const char* modeName() {
  return parameters.mode() == ControlMode::Frequency ? "frequency" : "K";
}

void applySwipe(long delta1Ms, long delta2Ms) {
  if ((delta1Ms == 0) || (delta2Ms == 0) ||
      ((delta1Ms > 0) != (delta2Ms > 0))) {
    return;
  }

  const int direction = delta1Ms < 0 ? 1 : -1;
  const uint32_t durationMs =
      static_cast<uint32_t>(labs(delta1Ms) + labs(delta2Ms));

  if (!parameters.applySwipe(direction, durationMs)) {
    return;
  }

  oscillators.setBaseFrequency(parameters.frequencyHz());
  oscillators.setCoupling(parameters.coupling());
}

void resetSwipe() {
  touchTime1 = 0;
  touchTime2 = 0;
  touchTime3 = 0;
}

void updateControls(uint32_t nowMs) {
  if (modeButton.pressed()) {
    parameters.toggleMode();
    resetSwipe();
    Serial.print("Control mode: ");
    Serial.println(modeName());
  }

  if (swipeButton1.pressed()) touchTime1 = nowMs;
  if (swipeButton2.pressed()) touchTime2 = nowMs;
  if (swipeButton3.pressed()) touchTime3 = nowMs;

  if (touchTime1 != 0 && touchTime2 != 0 && touchTime3 != 0) {
    const long delta1Ms =
        static_cast<long>(touchTime2) - static_cast<long>(touchTime1);
    const long delta2Ms =
        static_cast<long>(touchTime3) - static_cast<long>(touchTime2);

    if ((delta1Ms > 0 && delta2Ms > 0) ||
        (delta1Ms < 0 && delta2Ms < 0)) {
      applySwipe(delta1Ms, delta2Ms);
      resetSwipe();
    }
  }

  if (touchTime1 != 0 && nowMs - touchTime1 > Config::SwipeTimeoutMs) {
    touchTime1 = 0;
  }
  if (touchTime2 != 0 && nowMs - touchTime2 > Config::SwipeTimeoutMs) {
    touchTime2 = 0;
  }
  if (touchTime3 != 0 && nowMs - touchTime3 > Config::SwipeTimeoutMs) {
    touchTime3 = 0;
  }
}

void printDiagnostics(uint32_t nowMs) {
  if (nowMs - lastDiagnosticsMs < Config::DiagnosticsIntervalMs) {
    return;
  }
  lastDiagnosticsMs = nowMs;

  Serial.print("min:0, max:1, "); // To keep y-scale in plotting constant
  Serial.print("RelativeCoupling:");
  Serial.print(parameters.coupling() / Config::Parameters.coupling.maximum, 3);
  Serial.print(", RelativeFrequency:");
  Serial.print(parameters.frequencyHz() / Config::Parameters.frequency.maximum, 3);
  Serial.print(", OrderParameter:");
  Serial.println(oscillators.orderParameter(), 3);
}

void setup() {
  Serial.begin(Config::SerialBaud);

  oscillators.begin();
  oscillators.setBaseFrequency(parameters.frequencyHz());
  oscillators.setCoupling(parameters.coupling());

  Serial.println("Control mode: frequency");
}

void loop() {
  const uint32_t nowMs = millis();

  updateControls(nowMs);
  oscillators.update(micros());
  printDiagnostics(nowMs);
  delay(Config::LoopDelayMs);
}
