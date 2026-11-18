#include "PlaceStateMachine.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
  PlaceStateMachine::Config cfg{17,17,6,0,60000,30000,350};
  PlaceStateMachine sm(cfg);
  assert(sm.state() == PlaceStateMachine::State::IDLE);
  assert(sm.board().size() == 289);
  assert(sm.scanCard("AA01",1000) == PlaceStateMachine::ScanResult::ACCEPTED);
  auto p = sm.previewPosition();
  assert(p.x == 8 && p.y == 8);
  assert(sm.setPreviewPosition(16,16,1050));
  assert(sm.changeColor(1,1080));
  assert(sm.selectedColor() == 1);
  assert(sm.confirm(1100));
  assert(sm.state() == PlaceStateMachine::State::COMMITTING);
  sm.finishCommit(true);
  assert(sm.state() == PlaceStateMachine::State::IDLE);
  assert(sm.board()[288] == 1);
  assert(sm.scanCard("AA01",2000) == PlaceStateMachine::ScanResult::COOLDOWN);
  sm.dismissCooldownNotice();
  assert(sm.scanCard("BB02",3000) == PlaceStateMachine::ScanResult::ACCEPTED);
  assert(sm.setPreviewPosition(16,16,3100));
  assert(sm.confirm(3200));
  sm.finishCommit(false);
  assert(sm.state() == PlaceStateMachine::State::ERROR);
  assert(sm.board()[288] == 1); // rollback on failure

  PlaceStateMachine timeout(cfg);
  assert(timeout.scanCard("AA01",100) == PlaceStateMachine::ScanResult::ACCEPTED);
  timeout.tick(30200);
  assert(timeout.state() == PlaceStateMachine::State::IDLE);
  assert(timeout.board()[144] == PlaceStateMachine::OFF);

  PlaceStateMachine restored(cfg);
  std::vector<uint8_t> saved(289, PlaceStateMachine::OFF);
  saved[0] = 5;
  assert(restored.restoreBoard(saved));
  assert(restored.board()[0] == 5);
  saved[1] = 99;
  assert(!restored.restoreBoard(saved));
  assert(restored.board()[1] == PlaceStateMachine::OFF);

  PlaceStateMachine rollover(cfg);
  assert(rollover.scanCard("AA01",UINT32_MAX-100) == PlaceStateMachine::ScanResult::ACCEPTED);
  rollover.tick(30050);
  assert(rollover.state() == PlaceStateMachine::State::IDLE);

  std::cout << "PASS: board, selection, commit, rollback, cooldown, timeout, restore, rollover\n";
}
