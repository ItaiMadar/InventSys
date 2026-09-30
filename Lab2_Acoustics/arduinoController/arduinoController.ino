const int BUZZER_PIN = 14;
const unsigned long BAUD_RATE = 115200;
// Change these GPIO values to match the seven LEDs, ordered flat to sharp.
const int LIGHT_PINS[7] = {15, 2, 4, 16, 17, 5, 18};

void stopBuzzer() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

void clearLights() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(LIGHT_PINS[i], LOW);
  }
}

void showGauge(int position) {
  clearLights();
  digitalWrite(LIGHT_PINS[position + 3], HIGH);
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  for (int i = 0; i < 7; i++) {
    pinMode(LIGHT_PINS[i], OUTPUT);
  }
  stopBuzzer();
  clearLights();

  Serial.begin(BAUD_RATE);
  Serial.setTimeout(100);
  Serial.println("READY");
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();

  if (command == "PING") {
    Serial.println("READY");
    return;
  }

  if (command == "STOP") {
    stopBuzzer();
    Serial.println("DONE");
    return;
  }

  if (command == "LEDS_OFF") {
    clearLights();
    Serial.println("DONE");
    return;
  }

  if (command.startsWith("LED,")) {
    int position = command.substring(4).toInt();
    if (position < -3 || position > 3) {
      Serial.println("ERROR gauge position out of range");
      return;
    }
    showGauge(position);
    Serial.println("DONE");
    return;
  }

  int comma = command.indexOf(',');
  if (comma < 1) {
    Serial.println("ERROR expected frequency,duration_ms");
    return;
  }

  long frequency = command.substring(0, comma).toInt();
  long durationMs = command.substring(comma + 1).toInt();

  if (frequency < 0 || frequency > 20000 || durationMs < 0 || durationMs > 60000) {
    Serial.println("ERROR value out of range");
    return;
  }

  if (frequency > 0 && durationMs > 0) {
    tone(BUZZER_PIN, frequency);
  } else {
    stopBuzzer();
  }

  delay(durationMs);
  stopBuzzer();
  Serial.println("DONE");
}
