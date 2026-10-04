#ifndef COCO_DOMAIN_PRESENCE_STATE_H_
#define COCO_DOMAIN_PRESENCE_STATE_H_
#include <optional>
#include <utility>

#include "coco/domain/context.h"
namespace coco {
class PresenceState {
 public:
  explicit PresenceState(std::string category) : category_(std::move(category)) {}
  std::optional<Event> Update(bool seen, float confidence, bool online, Clock::time_point now,
                              const std::string& source, const std::string& at);
  const std::string& state() const {
    return state_;
  }

 private:
  std::string category_, state_ = "unknown", pending_;
  Clock::time_point since_{};
};
}  // namespace coco

#endif  // COCO_DOMAIN_PRESENCE_STATE_H_
