#include "coco/application/context_service.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "coco/application/dashboard.h"
#include "coco/capture/camera_source.h"
#include "coco/domain/context_engine.h"
#include "coco/domain/utc_time.h"
#include "coco/perception/box_tracker.h"
#include "coco/perception/detector.h"
#include "coco/perception/pet_appearance.h"
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
  std::string current_jpeg;
  Clock::time_point portrait_received{};
  coco::PetAppearance appearance(config.pet_gallery);
  coco::ContextApi api(config.port, [&](const std::string& path) -> HttpResponse {
    Context snapshot;
    if (path.rfind("/v1/pet/thumbnail?sequence=", 0) == 0) {
      const auto value = path.substr(27);
      if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        return {400, "{\"error\":\"invalid_sequence\"}"};
      }
      uint64_t sequence;
      try {
        sequence = std::stoull(value);
      } catch (const std::exception&) {
        return {400, "{\"error\":\"invalid_sequence\"}"};
      }
      std::lock_guard<std::mutex> lock(context_mutex);
      if (current.health != "online" || current_jpeg.empty() ||
          current.pet_portrait.sequence != sequence ||
          Clock::now() - portrait_received > std::chrono::seconds(3)) {
        return {404, "{\"error\":\"thumbnail_expired\"}"};
      }
      return {200, current_jpeg, "image/jpeg"};
    }
    {
      std::lock_guard<std::mutex> lock(context_mutex);
      snapshot = current;
    }
    return DashboardResponse(path, config.web_root, snapshot, Clock::now(), store,
                             ParseUtcSeconds(UtcNow()));
  });
  coco::CameraSource source(config.source);
  coco::ContextEngine engine(config.source_id);
  coco::BoxTracker tracker;
  api.Start();
  source.Start();
  auto started = coco::Clock::now(), next = started;
  uint64_t inferred = 0;
  auto publish = [&](const std::vector<Event>& events, const std::string& at) {
    if (engine.context().health == "offline") {
      appearance.Clear();
    }
    for (const auto& event : events) {
      store.Append(event);
      std::cout << "[event] " << event.category << ' ' << event.state << ' ' << event.observed_at
                << '\n';
    }
    store.RecordContext(engine.context(), ParseUtcSeconds(at));
    std::lock_guard<std::mutex> lock(context_mutex);
    const auto previous_sequence = current.pet_portrait.sequence;
    current = engine.context();
    current.pet_portrait = appearance.portrait();
    current_jpeg = appearance.jpeg();
    if (current.pet_portrait.sequence != previous_sequence) {
      portrait_received = engine.context().updated;
    }
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
      std::vector<BoxDetection> boxes;
      size_t people = 0, cats = 0;
      float pc = 0, cc = 0;
      for (const auto& d : detections) {
        boxes.push_back(
            {d.class_id,
             d.confidence,
             {double(d.box.x) / frame->image.cols, double(d.box.y) / frame->image.rows,
              double(d.box.width) / frame->image.cols, double(d.box.height) / frame->image.rows}});
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
      if (now - frame->received < std::chrono::seconds(2)) {
        observation.tracks = tracker.Update(boxes, frame->received);
        appearance.Observe(*frame, observation.tracks);
      }
      publish(engine.Observe(observation, now), frame->observed_at);
    } else {
      auto at = UtcNow();
      publish(engine.Tick(now, at), at);
      if (engine.context().health == "offline") {
        tracker.Reset();
      }
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
