#include "coco/domain/presence_state.h"

#include <chrono>
#include <optional>
namespace coco {
std::optional<Event> PresenceState::Update(bool seen, float confidence, bool online,
                                           Clock::time_point now, const std::string& source,
                                           const std::string& at) {
  std::string desired = online ? (seen ? "visible" : "out_of_view") : "offline";
  if (desired == state_) {
    pending_.clear();
    return {};
  }
  if (desired != pending_) {
    pending_ = desired;
    since_ = now;
  }
  double delay = desired == "offline" ? 0 : (desired == "visible" ? 1 : 5);
  if (std::chrono::duration<double>(now - since_).count() < delay) {
    return {};
  }
  state_ = desired;
  pending_.clear();
  return Event{source, category_, state_, at, seen && online ? confidence : 0};
}
}  // namespace coco
