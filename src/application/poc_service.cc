#include "coco/application/poc_service.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "coco/application/dashboard.h"
#include "coco/domain/utc_time.h"
#include "coco/storage/event_store.h"
#include "coco/transport/context_api.h"
namespace coco {
int RunPocService(const PocConfig& config, const std::function<bool()>& stop_requested) {
  if (config.port < 1 || config.port > 65535 || config.web_root.empty() ||
      config.replay.input.empty() || config.replay.database.empty() ||
      config.replay.source_id.empty() || !std::isfinite(config.replay.speed) ||
      config.replay.speed <= 0 || config.replay.speed > 1000) {
    throw std::invalid_argument("Invalid POC configuration");
  }
  EventStore store(config.replay.database);
  Context current;
  current.source_id = config.replay.source_id;
  current.mode = "replay";
  std::mutex mutex;
  Clock::time_point virtual_now = Clock::time_point(std::chrono::seconds(1)),
                    sample_received = Clock::now();
  int64_t reference = ParseUtcSeconds(UtcNow());
  std::atomic<bool> sampled{false};
  std::exception_ptr failure;
  std::atomic<bool> finished{false}, cancel{false};
  auto clock_snapshot = [&](Context& snapshot, Clock::time_point& clock, int64_t& utc) {
    std::lock_guard<std::mutex> lock(mutex);
    double elapsed =
        std::chrono::duration<double>(Clock::now() - sample_received).count() * config.replay.speed;
    snapshot = current;
    clock = virtual_now +
            std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(elapsed));
    utc = reference + int64_t(elapsed);
  };
  ContextApi api(config.port, [&](const std::string& path) -> HttpResponse {
    Context snapshot;
    Clock::time_point clock;
    int64_t utc;
    clock_snapshot(snapshot, clock, utc);
    return DashboardResponse(path, config.web_root, snapshot, clock, store, utc);
  });
  ReplayConfig replay = config.replay;
  replay.realtime = true;
  replay.stop_requested = [&] { return cancel.load(); };
  replay.on_sample = [&](const Context& context, Clock::time_point now, const std::string& at) {
    std::lock_guard<std::mutex> lock(mutex);
    current = context;
    virtual_now = now;
    reference = ParseUtcSeconds(at);
    sample_received = Clock::now();
    sampled = true;
  };
  api.Start();
  std::thread worker([&] {
    try {
      std::ostringstream discarded;
      RunReplayService(replay, discarded);
    } catch (...) {
      std::lock_guard<std::mutex> lock(mutex);
      failure = std::current_exception();
    }
    finished = true;
  });
  std::exception_ptr pending_error;
  try {
    std::cout << "[poc] simulation only: http://127.0.0.1:" << config.port << "\n";
    int64_t last_recorded = -1;
    while (!stop_requested()) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (failure) {
          std::rethrow_exception(failure);
        }
      }
      if (finished) {
        Context snapshot;
        Clock::time_point clock;
        int64_t utc;
        clock_snapshot(snapshot, clock, utc);
        if (sampled && clock - snapshot.updated >= std::chrono::seconds(5) && utc > last_recorded) {
          if (snapshot.health != "offline") {
            store.Append({snapshot.source_id, "pet", "offline", FormatUtcSeconds(utc), 0});
          }
          snapshot.health = "offline";
          snapshot.pet_state = "offline";
          snapshot.pet_confidence = 0;
          snapshot.pet_evidence = "offline";
          snapshot.pet_last_seen_seconds = -1;
          snapshot.cat = "offline";
          snapshot.person = "offline";
          snapshot.cat_count = 0;
          snapshot.person_count = 0;
          snapshot.tracks.clear();
          store.RecordContext(snapshot, utc);
          last_recorded = utc;
          std::lock_guard<std::mutex> lock(mutex);
          current = snapshot;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  } catch (...) {
    pending_error = std::current_exception();
  }
  cancel = true;
  worker.join();
  api.Stop();
  if (pending_error) {
    std::rethrow_exception(pending_error);
  }
  return 0;
}
}  // namespace coco
