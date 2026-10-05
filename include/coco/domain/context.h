#ifndef COCO_DOMAIN_CONTEXT_H_
#define COCO_DOMAIN_CONTEXT_H_
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "coco/domain/clock.h"
#include "coco/domain/tracking.h"

namespace coco {

struct Event {
  std::string source_id, category, state, observed_at;
  float confidence = 0;
};
struct Context {
  std::string source_id, observed_at, health = "starting";
  std::string person = "unknown", cat = "unknown";
  uint64_t sequence = 0, dropped_frames = 0;
  size_t person_count = 0, cat_count = 0;
  std::vector<TrackSnapshot> tracks;
  double inference_ms = 0;
  Clock::time_point updated{};
};
std::string UtcNow();
std::string JsonString(const std::string& value);
std::string ContextJson(const Context& context, Clock::time_point now = Clock::now());
}  // namespace coco

#endif  // COCO_DOMAIN_CONTEXT_H_
