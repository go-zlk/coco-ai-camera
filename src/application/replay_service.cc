#include "coco/application/replay_service.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "coco/domain/context_engine.h"
#include "coco/storage/event_store.h"
#include "tracking_replay.h"
namespace coco {
namespace {
constexpr const char* kHeader =
    "elapsed_ms,kind,person_count,cat_count,person_confidence,cat_confidence,observed_at";
std::vector<std::string> Fields(const std::string& line) {
  std::vector<std::string> fields;
  std::istringstream input(line);
  std::string value;
  while (std::getline(input, value, ',')) {
    fields.push_back(value);
  }
  return fields;
}
uint64_t Unsigned(const std::string& value) {
  if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
    throw std::invalid_argument("Expected unsigned decimal value");
  }
  return std::stoull(value);
}
float Confidence(const std::string& value) {
  size_t end = 0;
  float result = std::stof(value, &end);
  if (end != value.size() || !std::isfinite(result) || result < 0 || result > 1) {
    throw std::invalid_argument("Confidence must be in [0,1]");
  }
  return result;
}
}  // namespace
size_t RunReplayService(const ReplayConfig& config, std::ostream& output) {
  if (config.input.empty() || config.database.empty() || config.source_id.empty()) {
    throw std::invalid_argument("Replay requires input, database and source_id");
  }
  std::ifstream input(config.input);
  if (!input) {
    throw std::runtime_error("Cannot open replay input");
  }
  std::string line;
  auto read_line = [&] {
    if (!std::getline(input, line)) {
      return false;
    }
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    return true;
  };
  if (!read_line()) {
    throw std::invalid_argument("Missing replay header");
  }
  if (line == kTrackingReplayHeader) {
    return RunTrackingReplay(config, output);
  }
  if (line != kHeader) {
    throw std::invalid_argument("Unexpected replay CSV header");
  }
  EventStore store(config.database);
  // Nonzero epoch preserves Context's uninitialized-time sentinel.
  const auto epoch = Clock::time_point(std::chrono::seconds(1));
  ContextEngine engine(config.source_id, epoch);
  uint64_t previous_ms = 0, sequence = 0;
  size_t rows = 0;
  while (read_line()) {
    auto fields = Fields(line);
    if (fields.size() != 7 || fields[6].empty()) {
      throw std::invalid_argument("Replay rows require seven nonempty columns");
    }
    uint64_t elapsed = Unsigned(fields[0]);
    if (elapsed < previous_ms || elapsed > 86400000) {
      throw std::invalid_argument("Replay time must be nondecreasing within one day");
    }
    auto now = epoch + std::chrono::milliseconds(elapsed);
    size_t people = Unsigned(fields[2]), cats = Unsigned(fields[3]);
    float person_confidence = Confidence(fields[4]), cat_confidence = Confidence(fields[5]);
    std::vector<Event> events;
    if (fields[1] == "frame") {
      Observation observation;
      observation.source_id = config.source_id;
      observation.observed_at = fields[6];
      observation.received = now;
      observation.sequence = ++sequence;
      observation.person_count = people;
      observation.cat_count = cats;
      observation.person_confidence = person_confidence;
      observation.cat_confidence = cat_confidence;
      events = engine.Observe(observation, now);
    } else if (fields[1] == "tick" && people == 0 && cats == 0 && person_confidence == 0 &&
               cat_confidence == 0) {
      events = engine.Tick(now, fields[6]);
    } else {
      throw std::invalid_argument("Expected frame or tick with zero detection values");
    }
    for (const auto& event : events) {
      store.Append(event);
    }
    output << "{\"mode\":\"replay\",\"elapsed_ms\":" << elapsed
           << ",\"transitions\":" << events.size()
           << ",\"context\":" << ContextJson(engine.context(), now) << "}\n";
    previous_ms = elapsed;
    ++rows;
  }
  if (input.bad() || rows == 0) {
    throw std::runtime_error("Replay contains no rows or input read failed");
  }
  return rows;
}
}  // namespace coco
