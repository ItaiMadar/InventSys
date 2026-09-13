#include "Config.h"
#include "InputController.h"
#include "KuramotoLedSystem.h"
#include "ParameterController.h"

float frequencyOffsets[Config::OscillatorCount];


InputController inputs(Config::Input);
ParameterController parameters(Config::Parameters);
KuramotoLedSystem oscillators(Config::LedPins, frequencyOffsets);

uint32_t lastDiagnosticsMs = 0;

const char* modeName() {
  return parameters.mode() == ControlMode::Frequency ? "frequency" : "K";
}

void handleInput(const InputEvent& event) {
  if (!parameters.handle(event)) {
    return;
  }

  oscillators.setBaseFrequency(parameters.frequencyHz());
  oscillators.setCoupling(parameters.coupling());

  if (event.type == InputEventType::ToggleMode) {
    Serial.print("Control mode: ");
    Serial.println(modeName());
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

  handleInput(inputs.update(nowMs));
  oscillators.update(micros());
  printDiagnostics(nowMs);
}
