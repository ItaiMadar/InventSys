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
  ControlEvents readControls();

  void renderBoardAndPreview(
      const std::vector<uint8_t>& board,
      bool previewActive,
      PlaceStateMachine::Point previewPosition,
      uint8_t previewDisplayColor);

  // This is the only function that may know the physical LED strip topology.
  int mapCoordinateToLedIndex(int x, int y) const;

  void showIdleScreen();
  void showPlacementScreen(uint8_t color, PlaceStateMachine::Point position);
  void showCooldownScreen(const std::string& uid, uint32_t remainingMs);
  void showErrorScreen(const char* message);

  bool loadBoardSnapshot(std::vector<uint8_t>& board);
  bool saveBoardSnapshotAtomically(const std::vector<uint8_t>& board);
};
