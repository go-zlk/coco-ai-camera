#include "coco/application/context_service.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "coco/capture/camera_source.h"
#include "coco/domain/context_engine.h"
#include "coco/perception/detector.h"
#include "coco/storage/event_store.h"
#include "coco/transport/context_api.h"
namespace coco {
int RunContextService(const ServiceConfig& config, const std::function<bool()>& stop_requested) {
  ValidateServiceConfig(config);
  auto detector =
      coco::MakeDetector(config.backend, config.model, config.image_size, config.confidence);
  coco::EventStore store(config.database);
  coco::Context current;
  current.source_id = config.source_id;
  std::mutex context_mutex;
  coco::ContextApi api(config.port, [&](const std::string& path) -> std::string {
    if (path == "/v1/events") {
      return store.Timeline();
    }
    if (path == "/v1/context/current" || path == "/health") {
      std::lock_guard<std::mutex> lock(context_mutex);
      return coco::ContextJson(current);
    }
    return {};
  });
  coco::CameraSource source(config.source);
  coco::ContextEngine engine(config.source_id);
  api.Start();
  source.Start();
  auto started = coco::Clock::now(), next = started;
  uint64_t inferred = 0;
  auto publish = [&](const std::vector<Event>& events) {
    for (const auto& event : events) {
      store.Append(event);
      std::cout << "[event] " << event.category << ' ' << event.state << ' ' << event.observed_at
                << '\n';
    }
    std::lock_guard<std::mutex> lock(context_mutex);
    current = engine.context();
  };
  std::cout << "[service] local API port " << config.port << "; raw media storage disabled\n";
  while (!stop_requested() && (config.duration_seconds == 0 ||
                               std::chrono::duration<double>(coco::Clock::now() - started).count() <
                                   config.duration_seconds)) {
    auto frame = source.Take(std::chrono::milliseconds(100));
    auto now = coco::Clock::now();
    if (frame && std::chrono::duration<double>(now - frame->received).count() < 2) {
      engine.FrameReceived(frame->received, now);
      if (now < next) {
        continue;
      }
      auto begin = coco::Clock::now();
      auto detections = detector->Infer(*frame);
      now = coco::Clock::now();
      next = now + std::chrono::milliseconds(config.interval_ms);
      ++inferred;
      size_t people = 0, cats = 0;
      float pc = 0, cc = 0;
      for (const auto& d : detections) {
        if (d.class_id == 0) {
          ++people;
          pc = std::max(pc, d.confidence);
        }
        if (d.class_id == 15) {
          ++cats;
          cc = std::max(cc, d.confidence);
        }
      }
      coco::Observation observation;
      observation.source_id = config.source_id;
      observation.observed_at = frame->observed_at;
      observation.received = frame->received;
      observation.sequence = frame->sequence;
      observation.dropped_frames = source.dropped();
      observation.person_count = people;
      observation.cat_count = cats;
      observation.person_confidence = pc;
      observation.cat_confidence = cc;
      observation.inference_ms = std::chrono::duration<double, std::milli>(now - begin).count();
      publish(engine.Observe(observation, now));
    } else {
      publish(engine.Tick(now, coco::UtcNow()));
    }
  }

  source.Stop();
  api.Stop();
  std::cout << "[summary] inference_checks=" << inferred << " dropped_frames=" << source.dropped()
            << '\n';
  if (inferred == 0) {
    return 2;
  }
  return 0;
}
}  // namespace coco
