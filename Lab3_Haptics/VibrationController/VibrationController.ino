#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Presentation hardware pin map.
constexpr uint8_t VIB_PIN = 13;
constexpr uint8_t DIRECTION_PIN_1 = 12;
constexpr uint8_t DIRECTION_PIN_2 = 14;
constexpr uint8_t LED_PINS[] = {17, 16, 4, 2, 15};
constexpr size_t LED_COUNT = sizeof(LED_PINS) / sizeof(LED_PINS[0]);

// PWM controls motor drive strength. Pulse frequency controls how often that
// drive is gated on and off; it is not the PWM carrier frequency or motor RPM.
constexpr uint8_t MIN_ACTIVE_PWM = 150;
constexpr uint8_t MAX_PWM = 255;
constexpr float MIN_PULSE_FREQUENCY_HZ = 0.5f;
constexpr float MAX_PULSE_FREQUENCY_HZ = 20.0f;
constexpr uint32_t COMMAND_WATCHDOG_MS = 2000;
constexpr uint32_t MAX_COMMAND_DURATION_MS = 10000;

constexpr size_t SERIAL_BUFFER_SIZE = 96;

struct VibrationState {
  bool active = false;
  bool pulseOn = false;
  bool ledPulseOn = false;
  bool useWatchdog = true;
  uint8_t envelopePhase = 0;
  uint8_t pwmDuty = 0;
  float amplitude = 0.0f;
  float frequencyHz = 0.0f;
  uint32_t halfPeriodUs = 0;
  uint32_t phaseStartedUs = 0;
  uint32_t commandStartedMs = 0;
  uint32_t durationMs = 0;
  uint32_t lastCommandMs = 0;
};

VibrationState vibration;
char serialBuffer[SERIAL_BUFFER_SIZE];
size_t serialLength = 0;
bool discardSerialLine = false;

void writeMotorPwm(uint8_t duty) {
  analogWrite(VIB_PIN, duty);
}

void writeLedMeter(float amplitude, bool enabled) {
  const float filledSegments = constrain(amplitude, 0.0f, 1.0f) * LED_COUNT;
  for (size_t index = 0; index < LED_COUNT; ++index) {
    const float segmentFill =
        enabled ? constrain(filledSegments - index, 0.0f, 1.0f) : 0.0f;
    analogWrite(LED_PINS[index], static_cast<uint8_t>(lroundf(segmentFill * 255.0f)));
  }
}

void applyVibrationOutputs() {
  writeMotorPwm(vibration.pulseOn ? vibration.pwmDuty : 0);
  writeLedMeter(vibration.amplitude, vibration.ledPulseOn);
}

void stopVibration() {
  writeMotorPwm(0);
  writeLedMeter(0.0f, false);
  vibration.active = false;
  vibration.pulseOn = false;
  vibration.ledPulseOn = false;
  vibration.envelopePhase = 0;
  vibration.pwmDuty = 0;
  vibration.amplitude = 0.0f;
  vibration.frequencyHz = 0.0f;
  vibration.halfPeriodUs = 0;
  vibration.durationMs = 0;
}

uint8_t normalizedAmplitudeToPwm(float amplitude) {
  if (amplitude <= 0.0f) {
    return 0;
  }

  amplitude = constrain(amplitude, 0.0f, 1.0f);
  const float pwmRange = static_cast<float>(MAX_PWM - MIN_ACTIVE_PWM);
  return MIN_ACTIVE_PWM + static_cast<uint8_t>(lroundf(amplitude * pwmRange));
}

bool startVibration(float frequencyHz, float amplitude, uint32_t durationMs,
                    bool useWatchdog = true) {
  if (!isfinite(frequencyHz) || !isfinite(amplitude) ||
      frequencyHz < MIN_PULSE_FREQUENCY_HZ ||
      frequencyHz > MAX_PULSE_FREQUENCY_HZ || amplitude < 0.0f ||
      amplitude > 1.0f || durationMs > MAX_COMMAND_DURATION_MS) {
    return false;
  }

  if (amplitude == 0.0f) {
    stopVibration();
    return true;
  }

  const uint32_t nowUs = micros();
  const uint32_t nowMs = millis();
  const uint8_t requestedPwm = normalizedAmplitudeToPwm(amplitude);

  // A repeated indefinite command acts as a heartbeat without restarting the
  // pulse phase. This keeps the cadence smooth while the host refreshes it.
  if (vibration.active && vibration.useWatchdog && durationMs == 0 &&
      fabsf(vibration.frequencyHz - frequencyHz) < 0.0001f &&
      fabsf(vibration.amplitude - amplitude) < 0.0001f &&
      vibration.pwmDuty == requestedPwm) {
    vibration.lastCommandMs = nowMs;
    return true;
  }

  vibration.active = true;
  vibration.pulseOn = true;
  vibration.ledPulseOn = true;
  vibration.envelopePhase = 0;
  // Finite commands stop on their own. The watchdog protects indefinite
  // commands, which the host must refresh while a hover remains active.
  vibration.useWatchdog = useWatchdog && durationMs == 0;
  vibration.pwmDuty = requestedPwm;
  vibration.amplitude = amplitude;
  vibration.frequencyHz = frequencyHz;
  vibration.halfPeriodUs =
      static_cast<uint32_t>(500000.0f / frequencyHz);  // 50% pulse envelope.
  vibration.phaseStartedUs = nowUs;
  vibration.commandStartedMs = nowMs;
  vibration.durationMs = durationMs;
  vibration.lastCommandMs = nowMs;

  applyVibrationOutputs();
  return true;
}

void updateVibration() {
  if (!vibration.active) {
    return;
  }

  const uint32_t nowMs = millis();
  if ((vibration.durationMs > 0 &&
       static_cast<uint32_t>(nowMs - vibration.commandStartedMs) >=
           vibration.durationMs) ||
      (vibration.useWatchdog &&
       static_cast<uint32_t>(nowMs - vibration.lastCommandMs) >=
           COMMAND_WATCHDOG_MS)) {
    stopVibration();
    return;
  }

  // Defensive guard; active VIB commands always have a nonzero half-period.
  if (vibration.halfPeriodUs == 0) {
    return;
  }

  const uint32_t nowUs = micros();
  if (static_cast<uint32_t>(nowUs - vibration.phaseStartedUs) >=
      vibration.halfPeriodUs) {
    // Advance by one interval rather than assigning nowUs, which reduces drift.
    // If the loop was delayed for several intervals, resynchronize safely.
    const uint32_t elapsed = nowUs - vibration.phaseStartedUs;
    const uint32_t intervals = elapsed / vibration.halfPeriodUs;
    vibration.phaseStartedUs += intervals * vibration.halfPeriodUs;
    // Four motor half-periods form one LED cycle. Motor output alternates on
    // every half-period, while the LED meter stays on for two half-periods and
    // off for two. The resulting LED flash frequency is motor frequency / 2.
    vibration.envelopePhase =
        (vibration.envelopePhase + (intervals % 4U)) % 4U;
    vibration.pulseOn = (vibration.envelopePhase % 2U) == 0U;
    vibration.ledPulseOn = vibration.envelopePhase < 2U;
    applyVibrationOutputs();
  }
}

void printStatus() {
  Serial.print(F("STATUS active="));
  Serial.print(vibration.active ? 1 : 0);
  Serial.print(F(" pulse_on="));
  Serial.print(vibration.pulseOn ? 1 : 0);
  Serial.print(F(" led_on="));
  Serial.print(vibration.ledPulseOn ? 1 : 0);
  Serial.print(F(" frequency_hz="));
  Serial.print(vibration.frequencyHz, 3);
  Serial.print(F(" amplitude="));
  Serial.print(vibration.amplitude, 3);
  Serial.print(F(" pwm="));
  Serial.print(vibration.pwmDuty);
  Serial.print(F(" min_pwm="));
  Serial.print(MIN_ACTIVE_PWM);
  Serial.print(F(" max_pwm="));
  Serial.println(MAX_PWM);
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  VIB <frequency_hz> <amplitude_0_to_1> [duration_ms]"));
  Serial.println(F("  STOP"));
  Serial.println(F("  STATUS"));
  Serial.println(F("  HELP"));
  Serial.println(F("VIB duration 0 means run until STOP, with a 2 s command watchdog."));
  Serial.println(F("Repeat the VIB command to keep a continuous host-controlled effect alive."));
}

bool parseFloatStrict(const char *text, float &value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char *end = nullptr;
  value = strtof(text, &end);
  return end != text && *end == '\0' && isfinite(value);
}

bool parseUnsignedLongStrict(const char *text, uint32_t &value) {
  if (text == nullptr || *text == '\0' || *text == '-') {
    return false;
  }
  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  value = static_cast<uint32_t>(parsed);
  return true;
}

void reportCommandResult(bool success) {
  Serial.println(success ? F("OK") : F("ERR invalid command or value"));
}

void handleCommand(char *line) {
  char *savePointer = nullptr;
  char *command = strtok_r(line, " \t", &savePointer);
  if (command == nullptr) {
    return;
  }

  if (strcasecmp(command, "STOP") == 0) {
    stopVibration();
    Serial.println(F("OK"));
    return;
  }

  if (strcasecmp(command, "STATUS") == 0) {
    printStatus();
    return;
  }

  if (strcasecmp(command, "HELP") == 0) {
    printHelp();
    return;
  }

  if (strcasecmp(command, "VIB") == 0) {
    float frequencyHz = 0.0f;
    float amplitude = 0.0f;
    uint32_t durationMs = 0;
    const char *frequencyText = strtok_r(nullptr, " \t", &savePointer);
    const char *amplitudeText = strtok_r(nullptr, " \t", &savePointer);
    const char *durationText = strtok_r(nullptr, " \t", &savePointer);
    const char *extra = strtok_r(nullptr, " \t", &savePointer);

    const bool valid = parseFloatStrict(frequencyText, frequencyHz) &&
                       parseFloatStrict(amplitudeText, amplitude) &&
                       (durationText == nullptr ||
                        parseUnsignedLongStrict(durationText, durationMs)) &&
                       extra == nullptr &&
                       startVibration(frequencyHz, amplitude, durationMs);
    reportCommandResult(valid);
    return;
  }

  reportCommandResult(false);
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (discardSerialLine) {
      if (incoming == '\n') {
        discardSerialLine = false;
      }
      continue;
    }

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      serialBuffer[serialLength] = '\0';
      handleCommand(serialBuffer);
      serialLength = 0;
      continue;
    }

    if (serialLength < SERIAL_BUFFER_SIZE - 1) {
      serialBuffer[serialLength++] = incoming;
    } else {
      serialLength = 0;
      discardSerialLine = true;
      Serial.println(F("ERR command too long"));
    }
  }
}

void setup() {
  pinMode(VIB_PIN, OUTPUT);
  pinMode(DIRECTION_PIN_1, OUTPUT);
  pinMode(DIRECTION_PIN_2, OUTPUT);
  for (size_t index = 0; index < LED_COUNT; ++index) {
    pinMode(LED_PINS[index], OUTPUT);
  }

  // Direction is fixed; only VIB_PIN is modulated.
  digitalWrite(DIRECTION_PIN_1, HIGH);
  digitalWrite(DIRECTION_PIN_2, LOW);
  writeMotorPwm(0);
  writeLedMeter(0.0f, false);

  Serial.begin(115200);
  Serial.println(F("Vibration controller ready. Type HELP for commands."));
}

void loop() {
  readSerialCommands();
  updateVibration();
}
