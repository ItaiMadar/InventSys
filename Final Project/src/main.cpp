#include <Arduino.h>

#include "HardwareAdapter.h"
#include "PlaceStateMachine.h"
#include "ProjectConfig.h"

#include <string>
#include <vector>

namespace {

PlaceStateMachine::Config makeStateConfig() {
  PlaceStateMachine::Config config = {
      ProjectConfig::GRID_WIDTH,
      ProjectConfig::GRID_HEIGHT,
      ProjectConfig::COLOR_COUNT,
      ProjectConfig::DEFAULT_COLOR,
      ProjectConfig::COOLDOWN_TIME_MS,
      ProjectConfig::SESSION_TIMEOUT_MS,
      ProjectConfig::FLASH_INTERVAL_MS};
  return config;
}

HardwareAdapter hardware;
PlaceStateMachine stateMachine(makeStateConfig());

bool cooldownNoticeActive = false;
uint32_t cooldownNoticeStartedMs = 0;
std::string deniedUid;

void handleNfc(uint32_t nowMs) {
  if (stateMachine.state() != PlaceStateMachine::State::IDLE &&
      stateMachine.state() != PlaceStateMachine::State::COOLDOWN_DENIED)
    return;

  std::string uid;
  if (!hardware.readNfcUid(uid)) return;

  const PlaceStateMachine::ScanResult result =
      stateMachine.scanCard(uid, nowMs);
  if (result == PlaceStateMachine::ScanResult::ACCEPTED) {
    cooldownNoticeActive = false;
    deniedUid.clear();
  } else if (result == PlaceStateMachine::ScanResult::COOLDOWN) {
    deniedUid = uid;
    cooldownNoticeStartedMs = nowMs;
    cooldownNoticeActive = true;
  }
}

void handleCooldownNotice(uint32_t nowMs) {
  if (!cooldownNoticeActive ||
      stateMachine.state() != PlaceStateMachine::State::COOLDOWN_DENIED)
    return;

  if (nowMs - cooldownNoticeStartedMs >= ProjectConfig::COOLDOWN_NOTICE_MS) {
    stateMachine.dismissCooldownNotice();
    cooldownNoticeActive = false;
    deniedUid.clear();
  }
}

void handlePlacement(uint32_t nowMs) {
  if (stateMachine.state() != PlaceStateMachine::State::PLACING) return;

  const ControlEvents controls = hardware.readControls();
  if (controls.hasPosition)
    stateMachine.setPreviewPosition(controls.x, controls.y, nowMs);
  if (controls.colorDirection != 0)
    stateMachine.changeColor(controls.colorDirection, nowMs);

  if (!controls.confirmPressed || !stateMachine.confirm(nowMs)) return;

  const bool saved =
      hardware.saveBoardSnapshotAtomically(stateMachine.board());
  stateMachine.finishCommit(saved);
  if (!saved) hardware.showErrorScreen("Board snapshot failed; placement rolled back.");
}

void updateOutputs(uint32_t nowMs) {
  const PlaceStateMachine::State state = stateMachine.state();
  const bool placing = state == PlaceStateMachine::State::PLACING;
  const PlaceStateMachine::Point preview = stateMachine.previewPosition();
  const uint8_t previewColor =
      placing ? stateMachine.previewDisplayColor(nowMs) : PlaceStateMachine::OFF;

  hardware.renderBoardAndPreview(
      stateMachine.board(), placing, preview, previewColor);

  if (state == PlaceStateMachine::State::IDLE) {
    hardware.showIdleScreen();
  } else if (state == PlaceStateMachine::State::COOLDOWN_DENIED) {
    hardware.showCooldownScreen(
        deniedUid, stateMachine.cooldownRemainingMs(deniedUid, nowMs));
  } else if (state == PlaceStateMachine::State::PLACING) {
    hardware.showPlacementScreen(stateMachine.selectedColor(), preview);
  } else if (state == PlaceStateMachine::State::ERROR) {
    hardware.showErrorScreen("State machine error.");
  }
}

}  // namespace

void setup() {
  hardware.begin();

  std::vector<uint8_t> savedBoard;
  if (hardware.loadBoardSnapshot(savedBoard) &&
      !stateMachine.restoreBoard(savedBoard)) {
    hardware.showErrorScreen("Saved board snapshot is incompatible.");
  }

  updateOutputs(millis());
}

void loop() {
  const uint32_t nowMs = millis();

  handleNfc(nowMs);
  handleCooldownNotice(nowMs);
  handlePlacement(nowMs);
  stateMachine.tick(nowMs);
  updateOutputs(nowMs);
}
