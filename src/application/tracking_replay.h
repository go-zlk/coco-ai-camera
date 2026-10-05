#ifndef COCO_APPLICATION_TRACKING_REPLAY_H_
#define COCO_APPLICATION_TRACKING_REPLAY_H_
#include <cstddef>
#include <iosfwd>

#include "coco/application/replay_service.h"
namespace coco {
inline constexpr const char* kTrackingReplayHeader =
    "elapsed_ms,kind,class_id,confidence,x,y,width,height,observed_at";
size_t RunTrackingReplay(const ReplayConfig& config, std::ostream& output);
}  // namespace coco
#endif  // COCO_APPLICATION_TRACKING_REPLAY_H_
