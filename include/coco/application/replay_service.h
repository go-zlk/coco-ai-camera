#ifndef COCO_APPLICATION_REPLAY_SERVICE_H_
#define COCO_APPLICATION_REPLAY_SERVICE_H_
#include <cstddef>
#include <iosfwd>
#include <string>
namespace coco {
struct ReplayConfig {
  std::string input;
  std::string database = "data/replay_context.db";
  std::string source_id = "replay_camera";
};
// Virtual-clock replay, without camera, GPU, network or sleeps. JSON is marked
// mode=replay. Use a separate database. Bad CSV throws; prior rows may be committed.
size_t RunReplayService(const ReplayConfig& config, std::ostream& output);
}  // namespace coco
#endif  // COCO_APPLICATION_REPLAY_SERVICE_H_
