#pragma once
#include "coco/types.hpp"
#include <optional>
namespace coco {
class PresenceState {
public:
  explicit PresenceState(std::string category) : category_(std::move(category)) {}
  std::optional<Event> update(bool seen,float confidence,bool online,Clock::time_point now,
                              const std::string& source,const std::string& at);
  const std::string& state() const { return state_; }
private:
  std::string category_,state_="unknown",pending_;
  Clock::time_point since_{};
};
}
