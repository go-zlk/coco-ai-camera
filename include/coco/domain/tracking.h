#ifndef COCO_DOMAIN_TRACKING_H_
#define COCO_DOMAIN_TRACKING_H_

#include <cstdint>
#include <string>

#include "coco/domain/clock.h"

namespace coco {
// Coordinates are normalized to the frame, not pixels.
struct BoundingBox {
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;
};
struct BoxDetection {
  int class_id = 0;
  float confidence = 0;
  BoundingBox box;
};
struct TrackSnapshot {
  uint64_t track_id = 0;
  int class_id = 0;
  float confidence = 0;
  BoundingBox box;  // Last observed box, never a fabricated detection.
  std::string status = "tentative";
  bool observed = false;
  bool confirmed = false;
  Clock::time_point last_seen{};
};
}  // namespace coco
#endif  // COCO_DOMAIN_TRACKING_H_
