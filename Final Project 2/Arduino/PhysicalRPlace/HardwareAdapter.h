#pragma once

#include "PlaceStateMachine.h"

#include <cstdint>
#include <string>
#include <vector>

struct ControlEvents {
  bool hasPosition;
  int x;
  int y;
  int colorDirection;
  bool confirmPressed;
};

class HardwareAdapter {
 public:
  void begin();
  bool readNfcUid(std::string& uid);
  void resetControlsForNewSession();
  ControlEvents readControls();
  void renderBoardAndPreview(const std::vector<uint8_t>& board,
                             bool previewActive,
                             PlaceStateMachine::Point previewPosition,
                             uint8_t previewDisplayColor);
  int mapCoordinateToLedIndex(int x, int y) const;
  void showIdleScreen();
  void showPlacementScreen(uint8_t color, PlaceStateMachine::Point position);
  void showCooldownScreen(const std::string& uid, uint32_t remainingMs);
  void showErrorScreen(const char* message);
  bool loadBoardSnapshot(std::vector<uint8_t>& board);
  bool saveBoardSnapshotAtomically(const std::vector<uint8_t>& board);

 private:
  bool oledReady_ = false;
  bool storageReady_ = false;
  bool analogInitialized_ = false;
  uint32_t smoothX_ = 0;
  uint32_t smoothY_ = 0;
  int lastX_ = -1;
  int lastY_ = -1;
  bool lastRawButton_ = true;
  bool stableButton_ = true;
  bool buttonArmed_ = false;
  bool longPressFired_ = false;
  uint32_t buttonChangedAt_ = 0;
  uint32_t buttonPressedAt_ = 0;
  uint32_t lastDisplayAt_ = 0;
  uint32_t lastRenderAt_ = 0;
  std::string lastScreen_;
  std::vector<uint32_t> lastLedFrame_;
  void showScreen(const std::string& text);
};
