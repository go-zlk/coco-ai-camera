#ifndef COCO_APPLICATION_REPLAY_OUTPUT_H_
#define COCO_APPLICATION_REPLAY_OUTPUT_H_
#include <chrono>
#include <ostream>
#include <stdexcept>
#include <thread>

#include "coco/application/replay_service.h"
#include "coco/domain/utc_time.h"
#include "coco/storage/event_store.h"
namespace coco {
class ReplayOutput {
 public:
  explicit ReplayOutput(const ReplayConfig& config) : config_(config), started_(Clock::now()) {}
  void Emit(const Context& context, const std::vector<Event>& events, Clock::time_point now,
            const std::string& at, EventStore& store, std::ostream& output) {
    double elapsed =
        std::chrono::duration<double>(now - Clock::time_point(std::chrono::seconds(1))).count();
    if (config_.realtime) {
      auto target = started_ + std::chrono::duration_cast<Clock::duration>(
                                   std::chrono::duration<double>(elapsed / config_.speed));
      while (Clock::now() < target) {
        if (config_.stop_requested && config_.stop_requested()) {
          throw std::runtime_error("Replay stopped");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    store.RecordContext(context, ParseUtcSeconds(at));
    Context snapshot = context;
    snapshot.mode = "replay";
    output << "{\"mode\":\"replay\",\"elapsed_ms\":" << int64_t(elapsed * 1000)
           << ",\"transitions\":" << events.size() << ",\"context\":" << ContextJson(snapshot, now)
           << "}\n";
    if (config_.on_sample) {
      config_.on_sample(snapshot, now, at);
    }
  }

 private:
  const ReplayConfig& config_;
  Clock::time_point started_;
};
}  // namespace coco
#endif
