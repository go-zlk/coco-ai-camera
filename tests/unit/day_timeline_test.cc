#include <chrono>
#include <filesystem>
#include <stdexcept>

#include "coco/domain/utc_time.h"
#include "coco/storage/event_store.h"
namespace {
void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}
}  // namespace
int main() {
  auto path =
      std::filesystem::temp_directory_path() /
      ("coco-timeline-" + std::to_string(coco::Clock::now().time_since_epoch().count()) + ".db");
  auto t = coco::ParseUtcSeconds("2026-01-01T23:59:58Z");
  coco::Context context;
  context.source_id = "test";
  context.pet_state = "active";
  {
    coco::EventStore store(path.string());
    store.RecordContext(context, t);
    store.RecordContext(context, t + 1);
    store.RecordContext(context, t + 1);  // Duplicate is idempotent.
    auto day = store.DayTimeline("test", "2026-01-01", t + 3);
    Require(day.find("\"active\":2") != std::string::npos, "clip at midnight");
  }
  {
    coco::EventStore store(path.string());
    context.pet_state = "resting";
    store.RecordContext(context, t + 20);
    auto day = store.DayTimeline("test", "2026-01-02", t + 22);
    Require(day.find("\"active\":4") != std::string::npos, "bounded carry-over only");
    Require(day.find("\"resting\":2") != std::string::npos, "rest duration");
    Require(day.find("\"unknown\":14") != std::string::npos, "restart gap is unknown");
  }
  std::filesystem::remove(path);
  std::filesystem::remove(path.string() + "-wal");
  std::filesystem::remove(path.string() + "-shm");
  bool invalid = false;
  try {
    coco::ParseUtcSeconds("2026-02-30T00:00:00Z");
  } catch (const std::invalid_argument&) {
    invalid = true;
  }
  Require(invalid, "reject normalized invalid dates");
}
