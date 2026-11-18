#include "PlaceStateMachine.h"

#include <limits>

const uint8_t PlaceStateMachine::OFF;

namespace {
uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }
}

PlaceStateMachine::PlaceStateMachine(const Config& config)
    : config_(config),
      state_(State::IDLE),
      selectedColor_(config.defaultColor),
      replacedColor_(OFF),
      lastInputMs_(0),
      flashStartedMs_(0),
      pendingPlacementMs_(0) {
  preview_.x = 0;
  preview_.y = 0;

  const bool badSize = config.width <= 0 || config.height <= 0;
  const bool overflow = !badSize &&
      static_cast<std::size_t>(config.width) >
          std::numeric_limits<std::size_t>::max() /
              static_cast<std::size_t>(config.height);
  const bool badPalette = config.colorCount == 0 ||
                          config.defaultColor >= config.colorCount;
  if (badSize || overflow || badPalette || config.flashIntervalMs == 0) {
    state_ = State::ERROR;
    return;
  }

  board_.assign(static_cast<std::size_t>(config.width) *
                    static_cast<std::size_t>(config.height),
                OFF);
}

bool PlaceStateMachine::restoreBoard(const std::vector<uint8_t>& board) {
  if (state_ != State::IDLE || board.size() != board_.size()) return false;
  for (std::size_t i = 0; i < board.size(); ++i) {
    if (board[i] != OFF && board[i] >= config_.colorCount) return false;
  }
  board_ = board;
  return true;
}

PlaceStateMachine::ScanResult PlaceStateMachine::scanCard(
    const std::string& uid, uint32_t nowMs) {
  if (uid.empty()) return ScanResult::INVALID_UID;
  if (state_ != State::IDLE && state_ != State::COOLDOWN_DENIED)
    return ScanResult::BUSY;

  pruneCooldowns(nowMs);
  if (cooldownRemainingMs(uid, nowMs) != 0) {
    state_ = State::COOLDOWN_DENIED;
    return ScanResult::COOLDOWN;
  }

  activeUid_ = uid;
  preview_ = findStartingPoint();
  selectedColor_ = config_.defaultColor;
  lastInputMs_ = nowMs;
  flashStartedMs_ = nowMs;
  state_ = State::PLACING;
  return ScanResult::ACCEPTED;
}

void PlaceStateMachine::dismissCooldownNotice() {
  if (state_ == State::COOLDOWN_DENIED) state_ = State::IDLE;
}

bool PlaceStateMachine::setPreviewPosition(int x, int y, uint32_t nowMs) {
  if (state_ != State::PLACING || !validPoint(x, y)) return false;
  if (preview_.x == x && preview_.y == y) return false;
  preview_.x = x;
  preview_.y = y;
  lastInputMs_ = nowMs;
  flashStartedMs_ = nowMs;
  return true;
}

bool PlaceStateMachine::changeColor(int direction, uint32_t nowMs) {
  if (state_ != State::PLACING || direction == 0 || config_.colorCount < 2)
    return false;
  const int count = config_.colorCount;
  selectedColor_ = static_cast<uint8_t>(
      (static_cast<int>(selectedColor_) + (direction > 0 ? 1 : -1) + count) %
      count);
  lastInputMs_ = nowMs;
  flashStartedMs_ = nowMs;
  return true;
}

bool PlaceStateMachine::confirm(uint32_t nowMs) {
  if (state_ != State::PLACING) return false;
  const std::size_t index = indexOf(preview_.x, preview_.y);
  replacedColor_ = board_[index];
  board_[index] = selectedColor_;
  pendingPlacementMs_ = nowMs;
  state_ = State::COMMITTING;
  return true;
}

void PlaceStateMachine::finishCommit(bool snapshotSaved) {
  if (state_ != State::COMMITTING) return;
  if (!snapshotSaved) {
    board_[indexOf(preview_.x, preview_.y)] = replacedColor_;
    state_ = State::ERROR;
    return;
  }

  bool updated = false;
  for (std::size_t i = 0; i < cooldowns_.size(); ++i) {
    if (cooldowns_[i].uid == activeUid_) {
      cooldowns_[i].placedAtMs = pendingPlacementMs_;
      updated = true;
      break;
    }
  }
  if (!updated) cooldowns_.push_back(Cooldown{activeUid_, pendingPlacementMs_});
  clearSession();
  state_ = State::IDLE;
}

void PlaceStateMachine::tick(uint32_t nowMs) {
  pruneCooldowns(nowMs);
  if (state_ == State::PLACING &&
      elapsed(nowMs, lastInputMs_) >= config_.sessionTimeoutMs) {
    clearSession();
    state_ = State::IDLE;
  }
}

uint8_t PlaceStateMachine::previewDisplayColor(uint32_t nowMs) const {
  if (state_ != State::PLACING) return OFF;
  const bool showSelected =
      (elapsed(nowMs, flashStartedMs_) / config_.flashIntervalMs) % 2 == 0;
  return showSelected ? selectedColor_ : board_[indexOf(preview_.x, preview_.y)];
}

uint32_t PlaceStateMachine::cooldownRemainingMs(
    const std::string& uid, uint32_t nowMs) const {
  for (std::size_t i = 0; i < cooldowns_.size(); ++i) {
    if (cooldowns_[i].uid == uid) {
      const uint32_t age = elapsed(nowMs, cooldowns_[i].placedAtMs);
      return age < config_.cooldownMs ? config_.cooldownMs - age : 0;
    }
  }
  return 0;
}

bool PlaceStateMachine::validPoint(int x, int y) const {
  return x >= 0 && x < config_.width && y >= 0 && y < config_.height;
}

std::size_t PlaceStateMachine::indexOf(int x, int y) const {
  return static_cast<std::size_t>(y) * static_cast<std::size_t>(config_.width) +
         static_cast<std::size_t>(x);
}

PlaceStateMachine::Point PlaceStateMachine::findStartingPoint() const {
  const Point center = {config_.width / 2, config_.height / 2};
  Point best = center;
  long bestDistance = std::numeric_limits<long>::max();
  for (int y = 0; y < config_.height; ++y) {
    for (int x = 0; x < config_.width; ++x) {
      if (board_[indexOf(x, y)] != OFF) continue;
      const long dx = static_cast<long>(x) - center.x;
      const long dy = static_cast<long>(y) - center.y;
      const long distance = dx * dx + dy * dy;
      if (distance < bestDistance) {
        best = Point{x, y};
        bestDistance = distance;
      }
    }
  }
  return best;
}

void PlaceStateMachine::pruneCooldowns(uint32_t nowMs) {
  for (std::size_t i = 0; i < cooldowns_.size();) {
    if (elapsed(nowMs, cooldowns_[i].placedAtMs) >= config_.cooldownMs)
      cooldowns_.erase(cooldowns_.begin() + i);
    else
      ++i;
  }
}

void PlaceStateMachine::clearSession() {
  activeUid_.clear();
  replacedColor_ = OFF;
  pendingPlacementMs_ = 0;
}
