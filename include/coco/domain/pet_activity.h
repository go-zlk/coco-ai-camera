#ifndef COCO_DOMAIN_PET_ACTIVITY_H_
#define COCO_DOMAIN_PET_ACTIVITY_H_
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "coco/domain/context.h"
namespace coco {
// Single-cat visual movement heuristic, not sleep/health inference.
class PetActivity {
 public:
  std::optional<Event> Update(const Context& context, Clock::time_point now,
                              const std::string& observed_at);
  const std::string& state() const {
    return state_;
  }
  float confidence() const {
    return confidence_;
  }
  const std::string& evidence() const {
    return evidence_;
  }
  double last_seen_seconds() const {
    return last_seen_seconds_;
  }

 private:
  struct Sample {
    Clock::time_point at;
    BoundingBox box;
  };
  std::deque<Sample> samples_;
  uint64_t track_id_ = 0;
  std::string state_ = "unknown";
  std::optional<Clock::time_point> moving_since_;
  Clock::time_point changed_{};
  float confidence_ = 0;
  std::string evidence_ = "none";
  std::optional<Clock::time_point> last_confirmed_;
  double last_seen_seconds_ = -1;
};
}  // namespace coco
#endif  // COCO_DOMAIN_PET_ACTIVITY_H_
