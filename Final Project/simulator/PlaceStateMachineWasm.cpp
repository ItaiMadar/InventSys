#include "PlaceStateMachine.h"

#include <emscripten/emscripten.h>

#include <string>

namespace {
PlaceStateMachine* machine = 0;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int place_create(int width, int height, int colorCount,
                                     int defaultColor, uint32_t cooldownMs,
                                     uint32_t sessionTimeoutMs,
                                     uint32_t flashIntervalMs) {
  delete machine;
  PlaceStateMachine::Config config = {
      width, height, static_cast<uint8_t>(colorCount),
      static_cast<uint8_t>(defaultColor), cooldownMs, sessionTimeoutMs,
      flashIntervalMs};
  machine = new PlaceStateMachine(config);
  return machine->state() != PlaceStateMachine::State::ERROR;
}

EMSCRIPTEN_KEEPALIVE int place_state() {
  return machine ? static_cast<int>(machine->state()) : -1;
}

EMSCRIPTEN_KEEPALIVE int place_scan_card(const char* uid, uint32_t nowMs) {
  if (!machine) return static_cast<int>(PlaceStateMachine::ScanResult::BUSY);
  return static_cast<int>(
      machine->scanCard(uid ? std::string(uid) : std::string(), nowMs));
}

EMSCRIPTEN_KEEPALIVE void place_dismiss_cooldown_notice() {
  if (machine) machine->dismissCooldownNotice();
}

EMSCRIPTEN_KEEPALIVE int place_set_preview(int x, int y, uint32_t nowMs) {
  return machine && machine->setPreviewPosition(x, y, nowMs);
}

EMSCRIPTEN_KEEPALIVE int place_change_color(int direction, uint32_t nowMs) {
  return machine && machine->changeColor(direction, nowMs);
}

EMSCRIPTEN_KEEPALIVE int place_confirm(uint32_t nowMs) {
  return machine && machine->confirm(nowMs);
}

EMSCRIPTEN_KEEPALIVE void place_finish_commit(int snapshotSaved) {
  if (machine) machine->finishCommit(snapshotSaved != 0);
}

EMSCRIPTEN_KEEPALIVE void place_tick(uint32_t nowMs) {
  if (machine) machine->tick(nowMs);
}

EMSCRIPTEN_KEEPALIVE int place_preview_x() {
  return machine ? machine->previewPosition().x : -1;
}

EMSCRIPTEN_KEEPALIVE int place_preview_y() {
  return machine ? machine->previewPosition().y : -1;
}

EMSCRIPTEN_KEEPALIVE int place_selected_color() {
  return machine ? machine->selectedColor() : PlaceStateMachine::OFF;
}

EMSCRIPTEN_KEEPALIVE int place_preview_display_color(uint32_t nowMs) {
  return machine ? machine->previewDisplayColor(nowMs)
                 : PlaceStateMachine::OFF;
}

EMSCRIPTEN_KEEPALIVE int place_board_size() {
  return machine ? static_cast<int>(machine->board().size()) : 0;
}

EMSCRIPTEN_KEEPALIVE int place_board_color(int index) {
  if (!machine || index < 0 || index >= place_board_size())
    return PlaceStateMachine::OFF;
  return machine->board()[static_cast<std::size_t>(index)];
}

EMSCRIPTEN_KEEPALIVE uint32_t place_cooldown_remaining(const char* uid,
                                                       uint32_t nowMs) {
  if (!machine || !uid) return 0;
  return machine->cooldownRemainingMs(uid, nowMs);
}

}
