#include "tracking_replay.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "coco/domain/context_engine.h"
#include "coco/perception/box_tracker.h"
#include "coco/storage/event_store.h"

namespace coco {
namespace {
std::vector<std::string> Fields(const std::string& line) {
  std::vector<std::string> result;
  std::istringstream stream(line);
  std::string field;
  while (std::getline(stream, field, ',')) {
    result.push_back(field);
  }
  return result;
}
double Number(const std::string& field) {
  size_t end = 0;
  double number = std::stod(field, &end);
  if (end != field.size() || !std::isfinite(number)) {
    throw std::invalid_argument("Tracking replay requires finite numbers");
  }
  return number;
}
struct Sample {
  int64_t elapsed_ms = 0;
  std::string kind;
  std::string observed_at;
  std::vector<BoxDetection> detections;
};
}  // namespace
size_t RunTrackingReplay(const ReplayConfig& config, std::ostream& output) {
  std::ifstream input(config.input);
  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("Cannot open tracking replay input");
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  if (line != kTrackingReplayHeader) {
    throw std::invalid_argument("Unexpected tracking replay header");
  }
  EventStore store(config.database);
  auto epoch = Clock::time_point(std::chrono::seconds(1));
  ContextEngine engine(config.source_id, epoch);
  BoxTracker tracker;
  Sample sample;
  bool pending = false;
  uint64_t sequence = 0;
  size_t rows = 0;
  auto flush = [&] {
    auto now = epoch + std::chrono::milliseconds(sample.elapsed_ms);
    std::vector<Event> events;
    if (sample.kind == "tick") {
      events = engine.Tick(now, sample.observed_at);
      if (engine.context().health == "offline") {
        tracker.Reset();
      }
    } else {
      Observation observation;
      observation.source_id = config.source_id;
      observation.received = now;
      observation.observed_at = sample.observed_at;
      observation.sequence = ++sequence;
      observation.tracks = tracker.Update(sample.detections, now);
      for (const auto& detection : sample.detections) {
        if (detection.class_id == 0) {
          ++observation.person_count;
          observation.person_confidence =
              std::max(observation.person_confidence, detection.confidence);
        } else {
          ++observation.cat_count;
          observation.cat_confidence = std::max(observation.cat_confidence, detection.confidence);
        }
      }
      events = engine.Observe(observation, now);
    }
    for (const auto& event : events) {
      store.Append(event);
    }
    output << "{\"mode\":\"replay\",\"elapsed_ms\":" << sample.elapsed_ms
           << ",\"transitions\":" << events.size()
           << ",\"context\":" << ContextJson(engine.context(), now) << "}\n";
  };
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    auto fields = Fields(line);
    if (fields.size() != 9 || fields[8].empty()) {
      throw std::invalid_argument("Invalid box replay row");
    }
    if (fields[0].empty() || fields[0].find_first_not_of("0123456789") != std::string::npos) {
      throw std::invalid_argument("Invalid replay time");
    }
    int64_t elapsed = std::stoll(fields[0]);
    if (elapsed > 86400000 || (pending && elapsed < sample.elapsed_ms)) {
      throw std::invalid_argument("Replay time must be nondecreasing within one day");
    }
    if (fields[1] != "detection" && fields[1] != "empty" && fields[1] != "tick") {
      throw std::invalid_argument("Expected detection, empty or tick");
    }
    bool same_frame = pending && elapsed == sample.elapsed_ms;
    if (same_frame && (fields[1] != "detection" || sample.kind != "detection" ||
                       fields[8] != sample.observed_at)) {
      throw std::invalid_argument("Only detection rows with the same timestamp can share a frame");
    }
    // Validate marker values before committing the previous group.
    BoxDetection detection;
    if (fields[1] == "detection") {
      if (fields[2] != "0" && fields[2] != "15") {
        throw std::invalid_argument("Unsupported class");
      }
      detection.class_id = std::stoi(fields[2]);
      detection.confidence = Number(fields[3]);
      detection.box = {Number(fields[4]), Number(fields[5]), Number(fields[6]), Number(fields[7])};
    } else {
      if (fields[2] != "-1") {
        throw std::invalid_argument("Markers require class_id=-1");
      }
      for (size_t i = 3; i < 8; ++i) {
        if (Number(fields[i]) != 0) {
          throw std::invalid_argument("Markers require zero measurements");
        }
      }
    }
    if (!same_frame) {
      if (pending) {
        flush();
      }
      sample = {elapsed, fields[1], fields[8], {}};
      pending = true;
    }
    if (fields[1] == "detection") {
      sample.detections.push_back(detection);
    }
    ++rows;
  }
  if (input.bad() || !pending) {
    throw std::runtime_error("Empty or unreadable tracking replay");
  }
  flush();
  return rows;
}
}  // namespace coco
