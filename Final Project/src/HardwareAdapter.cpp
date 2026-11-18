#include "HardwareAdapter.h"

#include "ProjectConfig.h"

#include <Arduino.h>

void HardwareAdapter::begin() {
  Serial.begin(115200);

  // TODO: Configure knob ADC pins.
  // TODO: Configure debounced button input pins.
  // TODO: Initialize the selected addressable LED library for each strip.
  // TODO: Initialize one or two displays using the selected display library.
  // TODO: Initialize the NFC serial port with PIN_NFC_RX and PIN_NFC_TX.
  // TODO: Initialize Preferences/NVS or LittleFS when persistence is enabled.

  Serial.println("Hardware adapter started with placeholder peripherals.");
}

bool HardwareAdapter::readNfcUid(std::string& uid) {
  (void)uid;

  // TODO: Read one complete UID from the NFC reader's serial interface.
  // Normalize it to a stable string, assign it to uid, and return true once.
  // Return false while no complete scan is available. Never block here.
  return false;
}

ControlEvents HardwareAdapter::readControls() {
  ControlEvents events = {false, 0, 0, 0, false};

  // TODO: Read and filter both potentiometers.
  // Quantize them to integer X/Y cells. Set hasPosition only when the resulting
  // cell differs from the last reported cell so analog noise is not activity.
  //
  // TODO: Debounce both color buttons. Set colorDirection to +1 or -1 only on
  // a new press, never continuously while a button is held.
  //
  // TODO: Debounce the confirmation button and set confirmPressed once per
  // press. This function must remain non-blocking.

  return events;
}

void HardwareAdapter::renderBoardAndPreview(
    const std::vector<uint8_t>& board,
    bool previewActive,
    PlaceStateMachine::Point previewPosition,
    uint8_t previewDisplayColor) {
  (void)board;
  (void)previewActive;
  (void)previewPosition;
  (void)previewDisplayColor;

  // TODO: Convert every logical (x, y) using mapCoordinateToLedIndex().
  // Render committed palette indices from board. If previewActive is true,
  // render previewDisplayColor at previewPosition instead. The core already
  // alternates this value between the selected and underlying colors.
  // Push the completed frame to all LED strips without blocking.
}

int HardwareAdapter::mapCoordinateToLedIndex(int x, int y) const {
  (void)x;
  (void)y;

  // TODO: Implement only after strip count, direction, and serpentine/linear
  // wiring are known. Return the physical LED index for integer coordinates.
  return -1;
}

void HardwareAdapter::showIdleScreen() {
  // TODO: Display the NFC tap prompt.
}

void HardwareAdapter::showPlacementScreen(
    uint8_t color, PlaceStateMachine::Point position) {
  (void)color;
  (void)position;

  // TODO: Display the selected color and integer X/Y coordinate.
}

void HardwareAdapter::showCooldownScreen(
    const std::string& uid, uint32_t remainingMs) {
  (void)uid;
  (void)remainingMs;

  // TODO: Display denial and remaining cooldown time.
}

void HardwareAdapter::showErrorScreen(const char* message) {
  Serial.print("ERROR: ");
  Serial.println(message);

  // TODO: Display a recoverable error message on the selected screen(s).
}

bool HardwareAdapter::loadBoardSnapshot(std::vector<uint8_t>& board) {
  board.clear();
  if (!ProjectConfig::PERSISTENCE_ENABLED) return false;

  // TODO: Load a versioned snapshot, verify dimensions and checksum, then
  // populate board. Return false without modifying board if validation fails.
  return false;
}

bool HardwareAdapter::saveBoardSnapshotAtomically(
    const std::vector<uint8_t>& board) {
  (void)board;
  if (!ProjectConfig::PERSISTENCE_ENABLED) return true;

  // TODO: Write a complete versioned snapshot to a temporary NVS key or file,
  // verify it, then atomically replace the active snapshot. Never overwrite
  // the last valid snapshot until the replacement is complete.
  return false;
}
