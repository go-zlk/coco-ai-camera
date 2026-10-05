#include "coco/domain/context_engine.h"

#include <chrono>
#include <limits>
#include <stdexcept>
namespace {
void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}
}  // namespace
int main() {
  auto epoch = coco::Clock::time_point(std::chrono::seconds(1));
  coco::ContextEngine engine("test", epoch);
  auto sample = [&](int seconds, uint64_t sequence, size_t cats) {
    coco::Observation observation;
    observation.source_id = "test";
    observation.observed_at = "2026-01-01T00:00:00Z";
    observation.received = epoch + std::chrono::seconds(seconds);
    observation.sequence = sequence;
    observation.cat_count = cats;
    observation.cat_confidence = cats ? .9f : 0;
    return observation;
  };
  auto now = [&](int seconds) { return epoch + std::chrono::seconds(seconds); };
  Require(engine.Observe(sample(0, 1, 1), now(0)).empty(), "initial debounce");
  Require(engine.Observe(sample(1, 2, 1), now(1)).size() == 1, "visible confirmation");
  engine.Observe(sample(2, 3, 0), now(2));
  engine.Observe(sample(3, 4, 1), now(3));
  Require(engine.context().cat == "visible", "short miss suppressed");
  Require(engine.Observe(sample(4, 4, 0), now(4)).empty(), "duplicate ignored");
  Require(engine.context().cat_count == 1, "duplicate must not change count");
  Require(engine.Observe(sample(2, 9, 0), now(5)).empty(), "stale ignored");
  Require(engine.Observe(sample(2, 9, 0), now(2)).empty(), "time regression ignored");
  Require(engine.Observe(sample(9, 9, 0), now(8)).empty(), "future sample ignored");
  auto invalid = sample(4, 5, 1);
  invalid.cat_confidence = std::numeric_limits<float>::quiet_NaN();
  bool threw = false;
  try {
    engine.Observe(invalid, now(4));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  Require(threw, "invalid confidence rejected");
  invalid = sample(4, 5, 1);
  invalid.source_id = "another";
  threw = false;
  try {
    engine.Observe(invalid, now(4));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  Require(threw, "source isolation");
  engine.FrameReceived(now(6), now(6));
  Require(engine.Tick(now(8), "time").empty(), "capture heartbeat independent of inference");
  Require(engine.Tick(now(11), "time").size() == 3, "timeout emits both categories");
  Require(engine.Tick(now(12), "time").empty(), "offline event not repeated");
  Require(engine.context().updated == now(3), "offline preserves observation age");
  Require(engine.Observe(sample(12, 5, 1), now(12)).size() == 1, "recovery debounce");
  Require(engine.Observe(sample(13, 6, 1), now(13)).size() == 1, "recovery confirmation");
  Require(engine.context().health == "online" && engine.context().cat == "visible",
          "recovery state");
}
