#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class PlaceStateMachine {
 public:
  static const uint8_t OFF = 255;

  enum class State { IDLE, COOLDOWN_DENIED, PLACING, COMMITTING, ERROR };
  enum class ScanResult { ACCEPTED, COOLDOWN, BUSY, INVALID_UID };

  struct Config {
    int width;
    int height;
    uint8_t colorCount;
    uint8_t defaultColor;
    uint32_t cooldownMs;
    uint32_t sessionTimeoutMs;
    uint32_t flashIntervalMs;
  };

  struct Point {
    int x;
    int y;
  };

  explicit PlaceStateMachine(const Config& config);

  State state() const { return state_; }
  const std::vector<uint8_t>& board() const { return board_; }
  Point previewPosition() const { return preview_; }
  uint8_t selectedColor() const { return selectedColor_; }

  bool restoreBoard(const std::vector<uint8_t>& board);
  ScanResult scanCard(const std::string& uid, uint32_t nowMs);
  void dismissCooldownNotice();
  bool setPreviewPosition(int x, int y, uint32_t nowMs);
  bool changeColor(int direction, uint32_t nowMs);
  bool confirm(uint32_t nowMs);
  void finishCommit(bool snapshotSaved);
  void tick(uint32_t nowMs);

  uint8_t previewDisplayColor(uint32_t nowMs) const;
  uint32_t cooldownRemainingMs(const std::string& uid, uint32_t nowMs) const;

 private:
  struct Cooldown {
    std::string uid;
    uint32_t placedAtMs;
  };

  Config config_;
  State state_;
  std::vector<uint8_t> board_;
  std::vector<Cooldown> cooldowns_;
  std::string activeUid_;
  Point preview_;
  uint8_t selectedColor_;
  uint8_t replacedColor_;
  uint32_t lastInputMs_;
  uint32_t flashStartedMs_;
  uint32_t pendingPlacementMs_;

  bool validPoint(int x, int y) const;
  std::size_t indexOf(int x, int y) const;
  Point findStartingPoint() const;
  void pruneCooldowns(uint32_t nowMs);
  void clearSession();
};
