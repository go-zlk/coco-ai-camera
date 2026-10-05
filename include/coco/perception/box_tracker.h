#ifndef COCO_PERCEPTION_BOX_TRACKER_H_
#define COCO_PERCEPTION_BOX_TRACKER_H_

#include <chrono>
#include <cstddef>
#include <vector>

#include "coco/domain/tracking.h"
namespace coco {
struct TrackerConfig {
  double minimum_iou = 0.1;
  double maximum_center_distance = 0.12;
  std::chrono::milliseconds lost_after{3000};
  size_t confirmation_hits = 2;
};
// Geometry-only short-term association. One tracker per source and process.
// Single-thread owned; no appearance embedding or permanent identity guarantee.
class BoxTracker {
 public:
  explicit BoxTracker(TrackerConfig config = {});
  std::vector<TrackSnapshot> Update(const std::vector<BoxDetection>& detections,
                                    Clock::time_point received);
  // Clear temporal state on stream discontinuity. IDs are never reused in this instance.
  void Reset();

 private:
  struct Track {
    TrackSnapshot snapshot;
    size_t hits = 1;
    double velocity_x = 0;
    double velocity_y = 0;
  };
  TrackerConfig config_;
  std::vector<Track> tracks_;
  uint64_t next_id_ = 1;
  Clock::time_point last_update_{};
  bool started_ = false;
};
}  // namespace coco
#endif  // COCO_PERCEPTION_BOX_TRACKER_H_
