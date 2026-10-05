#ifndef COCO_APPLICATION_REPLAY_SERVICE_H_
#define COCO_APPLICATION_REPLAY_SERVICE_H_
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>

#include "coco/domain/context.h"
namespace coco {
struct ReplayConfig {
  std::string input;
  std::string database = "data/replay_context.db";
  std::string source_id = "replay_camera";
  bool realtime = false;
  std::function<bool()> stop_requested;
  double speed = 1;
  std::function<void(const Context&, Clock::time_point, const std::string&)> on_sample;
};
// Virtual-clock replay, without camera, GPU, network or sleeps. JSON is marked
// mode=replay. Use a separate database. Bad CSV throws; prior rows may be committed.
size_t RunReplayService(const ReplayConfig& config, std::ostream& output);
}  // namespace coco
#endif  // COCO_APPLICATION_REPLAY_SERVICE_H_
