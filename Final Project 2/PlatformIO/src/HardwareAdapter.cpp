#include "HardwareAdapter.h"
#include "HardwarePins.h"
#include "ProjectConfig.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {
MFRC522 rfid(HardwarePins::RFID_SS, HardwarePins::RFID_RST);
Adafruit_NeoPixel leds(ProjectConfig::LED_COUNT, HardwarePins::LED_DATA,
                      NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Preferences preferences;

const char* colorName(uint8_t index) {
  static const char* const names[] = {
      "White", "Red", "Green", "Blue", "Yellow", "Purple"};
  return index < ProjectConfig::COLOR_COUNT ? names[index] : "Off";
}

uint32_t paletteRgb(uint8_t index) {
  switch (index) {
    case ProjectConfig::COLOR_WHITE:  return Adafruit_NeoPixel::Color(255, 255, 255);
    case ProjectConfig::RED:    return Adafruit_NeoPixel::Color(255, 0, 0);
    case ProjectConfig::GREEN:  return Adafruit_NeoPixel::Color(0, 255, 0);
    case ProjectConfig::BLUE:   return Adafruit_NeoPixel::Color(0, 0, 255);
    case ProjectConfig::YELLOW: return Adafruit_NeoPixel::Color(255, 170, 0);
    case ProjectConfig::PURPLE: return Adafruit_NeoPixel::Color(150, 0, 255);
    default:                   return 0;
  }
}

// Two independent NVS slots: never overwrite the latest valid snapshot.
#pragma pack(push, 1)
struct SnapshotHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t width;
  uint16_t height;
  uint8_t paletteCount;
  uint8_t reserved;
  uint32_t sequence;
  uint32_t crc;
};
#pragma pack(pop)
static_assert(sizeof(SnapshotHeader) == 20, "Unexpected snapshot header layout");
constexpr uint32_t SNAPSHOT_MAGIC = 0x5247504c; // 'LPGR' numeric marker
constexpr uint16_t SNAPSHOT_VERSION = 1;

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j)
      crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
  }
  return crc;
}

uint32_t snapshotCrc(const SnapshotHeader& header,
                     const std::vector<uint8_t>& board) {
  uint32_t crc = 0xFFFFFFFFUL;
  crc = crc32Update(crc, reinterpret_cast<const uint8_t*>(&header),
                    offsetof(SnapshotHeader, crc));
  crc = crc32Update(crc, board.data(), board.size());
  return ~crc;
}

bool readSlot(const char* key, std::vector<uint8_t>& out,
              uint32_t& sequence) {
  const size_t expected = sizeof(SnapshotHeader) + ProjectConfig::LED_COUNT;
  if (preferences.getBytesLength(key) != expected) return false;
  std::vector<uint8_t> bytes(expected);
  if (preferences.getBytes(key, bytes.data(), bytes.size()) != expected)
    return false;

  SnapshotHeader header{};
  memcpy(&header, bytes.data(), sizeof(header));
  if (header.magic != SNAPSHOT_MAGIC || header.version != SNAPSHOT_VERSION ||
      header.width != ProjectConfig::GRID_WIDTH ||
      header.height != ProjectConfig::GRID_HEIGHT ||
      header.paletteCount != ProjectConfig::COLOR_COUNT)
    return false;

  std::vector<uint8_t> candidate(bytes.begin() + sizeof(header), bytes.end());
  if (snapshotCrc(header, candidate) != header.crc) return false;
  for (uint8_t color : candidate)
    if (color != PlaceStateMachine::OFF &&
        color >= ProjectConfig::COLOR_COUNT) return false;
  sequence = header.sequence;
  out.swap(candidate);
  return true;
}

bool seqNewer(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) > 0;
}

int resolveCell(uint32_t adc, int oldCell, int cellCount) {
  int candidate = static_cast<int>((adc * cellCount) / 4096UL);
  candidate = std::max(0, std::min(cellCount - 1, candidate));
  if (oldCell < 0 || candidate == oldCell) return candidate;
  if (candidate > oldCell) {
    const uint32_t boundary = static_cast<uint32_t>(oldCell + 1) *
                              4096UL / cellCount;
    if (adc < boundary + ProjectConfig::ADC_HYSTERESIS) return oldCell;
  } else {
    const int32_t boundary = static_cast<int32_t>(oldCell) *
                             4096L / cellCount;
    if (static_cast<int32_t>(adc) >
        boundary - static_cast<int32_t>(ProjectConfig::ADC_HYSTERESIS))
      return oldCell;
  }
  return candidate;
}
}

void HardwareAdapter::begin() {
  Serial.begin(115200);
  Serial.println("\nPhysical r/place booting...");

  // Ensure motor driver channels are disabled before activating output pins.
  const int motorPins[] = {HardwarePins::MOTOR_A_ENABLE,
    HardwarePins::MOTOR_A_IN1, HardwarePins::MOTOR_A_IN2,
    HardwarePins::MOTOR_B_ENABLE, HardwarePins::MOTOR_B_IN1,
    HardwarePins::MOTOR_B_IN2};
  for (int pin : motorPins) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }

  pinMode(HardwarePins::POSITION_X, INPUT);
  pinMode(HardwarePins::POSITION_Y, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(HardwarePins::POSITION_X, ADC_11db);
  analogSetPinAttenuation(HardwarePins::POSITION_Y, ADC_11db);
  pinMode(HardwarePins::CONFIRM_BUTTON, INPUT_PULLUP);
  resetControlsForNewSession();

  // RC522 uses VSPI on the pins in Fritzing, not Serial2/UART.
  SPI.begin(HardwarePins::RFID_SCK, HardwarePins::RFID_MISO,
            HardwarePins::RFID_MOSI, HardwarePins::RFID_SS);
  rfid.PCD_Init();

  Wire.begin(HardwarePins::OLED_SDA, HardwarePins::OLED_SCL);
  Wire.setClock(400000);
  int oledAddress = 0;
  for (const int address : {0x3C, 0x3D}) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) { oledAddress = address; break; }
  }
  if (oledAddress != 0) {
    oledReady_ = oled.begin(SSD1306_SWITCHCAPVCC, oledAddress);
    if (oledReady_) {
      oled.clearDisplay();
      oled.display();
      Serial.printf("OLED detected at 0x%02X\n", oledAddress);
    }
  }
  if (!oledReady_) Serial.println("OLED not found (0x3C/0x3D); serial still works.");

  leds.begin();
  leds.setBrightness(ProjectConfig::LED_BRIGHTNESS);
  leds.clear();
  leds.show();

  if (ProjectConfig::PERSISTENCE_ENABLED) {
    storageReady_ = preferences.begin("rplace", false);
    if (!storageReady_) Serial.println("ERROR: NVS storage unavailable.");
  }

  Serial.printf("Mode: %s | %d x %d | %d LEDs | GPIO%d data\n",
    ProjectConfig::TEST_MODE ? "6-LED TEST" : "17x17 GRID",
    ProjectConfig::GRID_WIDTH, ProjectConfig::GRID_HEIGHT,
    ProjectConfig::LED_COUNT, HardwarePins::LED_DATA);
  if (ProjectConfig::ENABLE_SERIAL_TEST_TAG)
    Serial.println("Test mode: send 't' over Serial Monitor to simulate card.");
  Serial.println("Single button: TAP = next color; HOLD 850ms = confirm.");
}

bool HardwareAdapter::readNfcUid(std::string& uid) {
  uid.clear();
  if (ProjectConfig::ENABLE_SERIAL_TEST_TAG) {
    while (Serial.available() > 0) {
      const char ch = static_cast<char>(Serial.read());
      if (ch == 't' || ch == 'T') {
        uid = "TEST_CARD";
        return true;
      }
    }
  }
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return false;
  static const char hex[] = "0123456789ABCDEF";
  for (byte i = 0; i < rfid.uid.size; ++i) {
    byte b = rfid.uid.uidByte[i];
    uid.push_back(hex[b >> 4]);
    uid.push_back(hex[b & 0x0F]);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return !uid.empty();
}

void HardwareAdapter::resetControlsForNewSession() {
  analogInitialized_ = false;
  lastX_ = -1;
  lastY_ = -1;
  stableButton_ = (digitalRead(HardwarePins::CONFIRM_BUTTON) != LOW);
  lastRawButton_ = stableButton_;
  buttonArmed_ = stableButton_;
  longPressFired_ = false;
  buttonChangedAt_ = millis();
  buttonPressedAt_ = millis();
}

ControlEvents HardwareAdapter::readControls() {
  ControlEvents events{false, 0, 0, 0, false};
  const uint32_t x = analogRead(HardwarePins::POSITION_X);
  const uint32_t y = analogRead(HardwarePins::POSITION_Y);
  if (!analogInitialized_) {
    smoothX_ = x;
    smoothY_ = y;
    analogInitialized_ = true;
  } else {
    smoothX_ = (smoothX_ * 3U + x) / 4U;
    smoothY_ = (smoothY_ * 3U + y) / 4U;
  }
  const int newX = resolveCell(smoothX_, lastX_, ProjectConfig::GRID_WIDTH);
  const int newY = resolveCell(smoothY_, lastY_, ProjectConfig::GRID_HEIGHT);
  if (newX != lastX_ || newY != lastY_) {
    lastX_ = newX;
    lastY_ = newY;
    events.hasPosition = true;
    events.x = newX;  // Core is ZERO-based; display adds 1.
    events.y = newY;
  }

  const uint32_t now = millis();
  const bool raw = digitalRead(HardwarePins::CONFIRM_BUTTON) != LOW;
  if (raw != lastRawButton_) {
    lastRawButton_ = raw;
    buttonChangedAt_ = now;
  }
  if (raw != stableButton_ &&
      static_cast<uint32_t>(now - buttonChangedAt_) >=
          ProjectConfig::BUTTON_DEBOUNCE_MS) {
    stableButton_ = raw;
    if (!stableButton_) {
      buttonPressedAt_ = now;
      longPressFired_ = false;
    } else {
      if (buttonArmed_ && !longPressFired_ &&
          static_cast<uint32_t>(now - buttonPressedAt_) <
              ProjectConfig::BUTTON_CONFIRM_HOLD_MS) {
        events.colorDirection = 1;
      }
      buttonArmed_ = true;
    }
  }
  if (!stableButton_ && buttonArmed_ && !longPressFired_ &&
      static_cast<uint32_t>(now - buttonPressedAt_) >=
          ProjectConfig::BUTTON_CONFIRM_HOLD_MS) {
    longPressFired_ = true;
    events.confirmPressed = true;
  }
  return events;
}

int HardwareAdapter::mapCoordinateToLedIndex(int x, int y) const {
  if (x < 0 || x >= ProjectConfig::GRID_WIDTH ||
      y < 0 || y >= ProjectConfig::GRID_HEIGHT) return -1;
  const bool leftToRight = (y % 2 == 0) ==
                            ProjectConfig::FIRST_ROW_LEFT_TO_RIGHT;
  const int column = leftToRight ? x : ProjectConfig::GRID_WIDTH - 1 - x;
  return y * ProjectConfig::GRID_WIDTH + column;
}

void HardwareAdapter::renderBoardAndPreview(
    const std::vector<uint8_t>& board, bool previewActive,
    PlaceStateMachine::Point previewPosition, uint8_t previewDisplayColor) {
  if (board.size() != static_cast<size_t>(ProjectConfig::LED_COUNT)) return;
  const uint32_t now = millis();
  if (lastRenderAt_ && static_cast<uint32_t>(now - lastRenderAt_) < 20)
    return; // Cap pixel rendering to 50 Hz; avoid repeated heap allocations.
  lastRenderAt_ = now;
  std::vector<uint32_t> next(ProjectConfig::LED_COUNT, 0);
  for (int y = 0; y < ProjectConfig::GRID_HEIGHT; ++y)
    for (int x = 0; x < ProjectConfig::GRID_WIDTH; ++x) {
      int physical = mapCoordinateToLedIndex(x, y);
      next[physical] = paletteRgb(board[y * ProjectConfig::GRID_WIDTH + x]);
    }
  if (previewActive) {
    int physical = mapCoordinateToLedIndex(previewPosition.x, previewPosition.y);
    if (physical >= 0) next[physical] = paletteRgb(previewDisplayColor);
  }
  if (next == lastLedFrame_) return; // Avoid unnecessary 800kHz transfers.
  for (int i = 0; i < ProjectConfig::LED_COUNT; ++i)
    leds.setPixelColor(i, next[i]);
  leds.show(); // ~0.18ms for 6 LEDs, ~8.7ms for 289 LEDs.
  lastLedFrame_.swap(next);
}

void HardwareAdapter::showScreen(const std::string& text) {
  if (!oledReady_) return;
  if (text == lastScreen_) return;
  uint32_t now = millis();
  if (lastDisplayAt_ != 0 &&
      static_cast<uint32_t>(now - lastDisplayAt_) <
          ProjectConfig::DISPLAY_MIN_UPDATE_MS) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print(text.c_str());
  oled.display();
  lastScreen_ = text;
  lastDisplayAt_ = now;
}

void HardwareAdapter::showIdleScreen() {
  showScreen("r/place LED board\n\nTap RFID card\nto begin\n\nTap=next color");
}

void HardwareAdapter::showPlacementScreen(uint8_t color,
                                           PlaceStateMachine::Point position) {
  char buf[130];
  snprintf(buf, sizeof(buf), "PLACE PIXEL\n\nX: %d   Y: %d\nColor: %s\n\nTap: next color\nHold: confirm",
           position.x + 1, position.y + 1, colorName(color));
  showScreen(buf);
}

void HardwareAdapter::showCooldownScreen(const std::string& uid,
                                          uint32_t remainingMs) {
  (void)uid;
  char buf[80];
  snprintf(buf, sizeof(buf), "COOLDOWN ACTIVE\n\nWait %lu seconds\n\nTap later",
           static_cast<unsigned long>((remainingMs + 999UL) / 1000UL));
  showScreen(buf);
}

void HardwareAdapter::showErrorScreen(const char* message) {
  std::string label = std::string("ERROR\n\n") + (message ? message : "Unknown");
  if (label != lastScreen_) Serial.println(label.c_str());
  showScreen(label);
}

bool HardwareAdapter::loadBoardSnapshot(std::vector<uint8_t>& board) {
  if (!ProjectConfig::PERSISTENCE_ENABLED || !storageReady_) return false;
  std::vector<uint8_t> a, b;
  uint32_t seqA = 0, seqB = 0;
  bool validA = readSlot("slotA", a, seqA);
  bool validB = readSlot("slotB", b, seqB);
  if (!validA && !validB) return false;
  board = (validB && (!validA || seqNewer(seqB, seqA))) ? b : a;
  Serial.printf("Loaded %u saved pixels from NVS.\n",
                static_cast<unsigned>(board.size()));
  return true;
}

bool HardwareAdapter::saveBoardSnapshotAtomically(
    const std::vector<uint8_t>& board) {
  if (!ProjectConfig::PERSISTENCE_ENABLED) return true;
  if (!storageReady_ ||
      board.size() != static_cast<size_t>(ProjectConfig::LED_COUNT)) return false;
  std::vector<uint8_t> a, b;
  uint32_t seqA = 0, seqB = 0;
  bool validA = readSlot("slotA", a, seqA);
  bool validB = readSlot("slotB", b, seqB);
  const bool latestIsB = validB && (!validA || seqNewer(seqB, seqA));
  const char* target = latestIsB ? "slotA" : (validA ? "slotB" : "slotA");
  const uint32_t nextSeq = (!validA && !validB) ? 1 :
      ((latestIsB ? seqB : seqA) + 1U);

  SnapshotHeader header{};
  header.magic = SNAPSHOT_MAGIC;
  header.version = SNAPSHOT_VERSION;
  header.width = ProjectConfig::GRID_WIDTH;
  header.height = ProjectConfig::GRID_HEIGHT;
  header.paletteCount = ProjectConfig::COLOR_COUNT;
  header.sequence = nextSeq;
  header.crc = snapshotCrc(header, board);
  std::vector<uint8_t> bytes(sizeof(header) + board.size());
  memcpy(bytes.data(), &header, sizeof(header));
  memcpy(bytes.data() + sizeof(header), board.data(), board.size());
  if (preferences.putBytes(target, bytes.data(), bytes.size()) != bytes.size())
    return false;
  std::vector<uint8_t> verified;
  uint32_t verifySeq = 0;
  const bool good = readSlot(target, verified, verifySeq) &&
                    verifySeq == nextSeq && verified == board;
  if (good) Serial.printf("Saved board snapshot #%lu.\n",
                          static_cast<unsigned long>(nextSeq));
  return good;
}
