#ifndef COCO_DOMAIN_CONTEXT_ENGINE_H_
#define COCO_DOMAIN_CONTEXT_ENGINE_H_

#include <string>
#include <vector>

#include "coco/domain/context.h"
#include "coco/domain/presence_state.h"

namespace coco {
// A successfully inferred sample. Independent of image format and inference backend.
struct Observation {
  std::string source_id;
  std::string observed_at;
  Clock::time_point received;
  uint64_t sequence = 0;
  uint64_t dropped_frames = 0;
  size_t person_count = 0;
  size_t cat_count = 0;
  float person_confidence = 0;
  float cat_confidence = 0;
  std::vector<TrackSnapshot> tracks;
  double inference_ms = 0;
};

// Single-source, single-writer state engine. Snapshot copying/synchronization is
// the caller's responsibility. No camera, model, database or network ownership.
class ContextEngine {
 public:
  explicit ContextEngine(std::string source_id, Clock::time_point started = Clock::now());
  // Capture heartbeat; independent of the inference sampling interval.
  void FrameReceived(Clock::time_point received, Clock::time_point now);
  // Returns transitions only. Duplicate, out-of-order and stale samples are ignored.
  // A foreign source or invalid numeric observation throws invalid_argument.
  std::vector<Event> Observe(const Observation& observation, Clock::time_point now);
  // Calls without new observations still detect capture timeout.
  std::vector<Event> Tick(Clock::time_point now, const std::string& observed_at);
  const Context& context() const {
    return context_;
  }

 private:
  Context context_;
  PresenceState person_{"person"};
  PresenceState cat_{"cat"};
  Clock::time_point last_frame_;
  Clock::time_point last_observation_{};
  bool has_observation_ = false;
};
}  // namespace coco
#endif  // COCO_DOMAIN_CONTEXT_ENGINE_H_
