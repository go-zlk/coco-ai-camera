#include "coco/domain/context_engine.h"

#include <chrono>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

namespace coco {
namespace {
constexpr auto kFrameMaxAge = std::chrono::seconds(2);
constexpr auto kCaptureTimeout = std::chrono::seconds(5);
void AddEvent(std::vector<Event>& events, std::optional<Event> event) {
  if (event) {
    events.push_back(std::move(*event));
  }
}
bool ValidConfidence(float value) {
  return std::isfinite(value) && value >= 0 && value <= 1;
}
}  // namespace

ContextEngine::ContextEngine(std::string source_id, Clock::time_point started)
    : last_frame_(started) {
  if (source_id.empty()) {
    throw std::invalid_argument("source_id must not be empty");
  }
  context_.source_id = std::move(source_id);
}

void ContextEngine::FrameReceived(Clock::time_point received, Clock::time_point now) {
  if (received <= now && now - received < kFrameMaxAge && received > last_frame_) {
    last_frame_ = received;
  }
}

std::vector<Event> ContextEngine::Observe(const Observation& observation, Clock::time_point now) {
  if (observation.source_id != context_.source_id) {
    throw std::invalid_argument("Observation belongs to another source");
  }
  if (observation.observed_at.empty() || !ValidConfidence(observation.person_confidence) ||
      !ValidConfidence(observation.cat_confidence) || !std::isfinite(observation.inference_ms) ||
      observation.inference_ms < 0 || observation.sequence == 0) {
    throw std::invalid_argument("Invalid observation");
  }
  if (observation.received > now || now - observation.received >= kFrameMaxAge ||
      (has_observation_ &&
       (observation.sequence <= context_.sequence || observation.received < last_observation_))) {
    return {};
  }
  FrameReceived(observation.received, now);
  std::vector<Event> events;
  AddEvent(events,
           person_.Update(observation.person_count > 0, observation.person_confidence, true,
                          observation.received, context_.source_id, observation.observed_at));
  AddEvent(events, cat_.Update(observation.cat_count > 0, observation.cat_confidence, true,
                               observation.received, context_.source_id, observation.observed_at));
  context_.health = "online";
  context_.person = person_.state();
  context_.cat = cat_.state();
  context_.person_count = observation.person_count;
  context_.cat_count = observation.cat_count;
  context_.sequence = observation.sequence;
  context_.dropped_frames = observation.dropped_frames;
  context_.observed_at = observation.observed_at;
  context_.updated = observation.received;
  context_.inference_ms = observation.inference_ms;
  context_.tracks = observation.tracks;
  last_observation_ = observation.received;
  has_observation_ = true;
  AddEvent(events, activity_.Update(context_, observation.received, observation.observed_at));
  context_.pet_state = activity_.state();
  context_.pet_confidence = activity_.confidence();
  return events;
}

std::vector<Event> ContextEngine::Tick(Clock::time_point now, const std::string& observed_at) {
  if (now - last_frame_ < kCaptureTimeout) {
    return {};
  }
  std::vector<Event> events;
  AddEvent(events, person_.Update(false, 0, false, now, context_.source_id, observed_at));
  AddEvent(events, cat_.Update(false, 0, false, now, context_.source_id, observed_at));
  context_.health = "offline";
  context_.person = person_.state();
  context_.cat = cat_.state();
  context_.tracks.clear();
  context_.person_count = 0;
  context_.cat_count = 0;
  AddEvent(events, activity_.Update(context_, now, observed_at));
  context_.pet_state = activity_.state();
  context_.pet_confidence = activity_.confidence();
  // Keep the last successful observation timestamp for truthful freshness.
  return events;
}
}  // namespace coco
