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
    const auto begin = coco::ParseUtcSeconds("2026-01-01T16:00:00Z");
    auto local = store.WindowTimeline("test", "2026-01-02", begin, begin + 86400, t + 22);
    Require(local.find("\"active\":6") != std::string::npos, "local day spans UTC midnight");
    Require(local.find("2026-01-01T16:00:00Z") != std::string::npos, "explicit UTC window");
    auto short_day =
        store.WindowTimeline("test", "2026-03-08", coco::ParseUtcSeconds("2026-03-08T05:00:00Z"),
                             coco::ParseUtcSeconds("2026-03-09T04:00:00Z"),
                             coco::ParseUtcSeconds("2026-03-10T00:00:00Z"));
    Require(short_day.find("\"unknown\":82800") != std::string::npos, "23 hour day");
    auto long_day =
        store.WindowTimeline("test", "2026-11-01", coco::ParseUtcSeconds("2026-11-01T04:00:00Z"),
                             coco::ParseUtcSeconds("2026-11-02T05:00:00Z"),
                             coco::ParseUtcSeconds("2026-11-03T00:00:00Z"));
    Require(long_day.find("\"day_seconds\":90000") != std::string::npos, "25 hour day");
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
