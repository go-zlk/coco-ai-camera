#include "coco/application/context_service.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>

#include "coco/capture/camera_source.h"
#include "coco/domain/presence_state.h"
#include "coco/perception/detector.h"
#include "coco/storage/event_store.h"
#include "coco/transport/context_api.h"
namespace {
volatile std::sig_atomic_t stopping = 0;
void HandleInterrupt(int) {
  stopping = 1;
}
}  // namespace
namespace coco {
int RunContextService(const ServiceConfig& config) {
  if (config.source.empty() || config.model.empty() || config.port < 1 || config.port > 65535 ||
      config.image_size < 32 || config.interval_ms < 1 || !std::isfinite(config.duration_seconds) ||
      config.duration_seconds < 0 || !std::isfinite(config.confidence) || config.confidence <= 0 ||
      config.confidence >= 1) {
    throw std::invalid_argument("Invalid service configuration");
  }
  stopping = 0;
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
  coco::PresenceState person("person"), cat("cat");
  std::signal(SIGINT, HandleInterrupt);
  std::signal(SIGTERM, HandleInterrupt);
  api.Start();
  source.Start();
  auto started = coco::Clock::now(), last_frame = started, next = started;
  uint64_t inferred = 0;
  auto emit = [&](std::optional<coco::Event> event) {
    if (event) {
      store.Append(*event);
      std::cout << "[event] " << event->category << ' ' << event->state << ' ' << event->observed_at
                << std::endl;
    }
  };
  std::cout << "[service] local API port " << config.port << "; raw media storage disabled\n";
  while (!stopping && (config.duration_seconds == 0 ||
                       std::chrono::duration<double>(coco::Clock::now() - started).count() <
                           config.duration_seconds)) {
    auto frame = source.Take(std::chrono::milliseconds(100));
    auto now = coco::Clock::now();
    if (frame && std::chrono::duration<double>(now - frame->received).count() < 2) {
      last_frame = frame->received;
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
      emit(person.Update(people > 0, pc, true, frame->received, current.source_id,
                         frame->observed_at));
      emit(cat.Update(cats > 0, cc, true, frame->received, current.source_id, frame->observed_at));
      std::lock_guard<std::mutex> lock(context_mutex);
      current.health = "online";
      current.person = person.state();
      current.cat = cat.state();
      current.person_count = people;
      current.cat_count = cats;
      current.sequence = frame->sequence;
      current.observed_at = frame->observed_at;
      current.updated = frame->received;
      current.inference_ms = std::chrono::duration<double, std::milli>(now - begin).count();
      current.dropped_frames = source.dropped();
    } else if (std::chrono::duration<double>(now - last_frame).count() >= 5) {
      auto at = coco::UtcNow();
      emit(person.Update(false, 0, false, now, current.source_id, at));
      emit(cat.Update(false, 0, false, now, current.source_id, at));
      std::lock_guard<std::mutex> lock(context_mutex);
      current.health = "offline";
      current.person = person.state();
      current.cat = cat.state();
      current.person_count = 0;
      current.cat_count = 0;
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
