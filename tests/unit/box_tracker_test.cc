#include "coco/perception/box_tracker.h"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
void Require(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}
coco::BoxDetection Cat(double x, double y = .2) {
  return {15, .9f, {x, y, .15, .2}};
}
}  // namespace
int main() {
  coco::BoxTracker tracker;
  auto epoch = coco::Clock::time_point(std::chrono::seconds(1));
  auto at = [&](int ms) { return epoch + std::chrono::milliseconds(ms); };
  auto tracks = tracker.Update({Cat(.1), Cat(.65)}, at(0));
  uint64_t first = tracks[0].track_id, second = tracks[1].track_id;
  Require(first != second && !tracks[0].confirmed, "distinct tentative tracks");
  tracks = tracker.Update({Cat(.62), Cat(.14)}, at(1000));
  Require(tracks[0].track_id == first && tracks[0].box.x == .14 && tracks[0].confirmed,
          "input reorder retains ID");
  Require(tracks[1].track_id == second && tracks[1].box.x == .62, "second cat stable");
  tracks = tracker.Update({Cat(.59)}, at(2000));
  Require(tracks[0].status == "lost" && !tracks[0].observed, "lost is not a new observation");
  tracks = tracker.Update({Cat(.22), Cat(.56)}, at(3000));
  Require(tracks.size() == 2 && tracks[0].track_id == first && tracks[0].observed,
          "short occlusion recovers ID");
  tracks = tracker.Update({}, at(4000));
  Require(tracks.size() == 2 && !tracks[0].observed, "empty frame retains short history");
  tracks = tracker.Update({Cat(.25)}, at(6000));
  Require(tracks.size() == 1 && tracks[0].track_id != first, "expired ID not revived");
  uint64_t prior = tracks[0].track_id;
  tracker.Reset();
  tracks = tracker.Update({Cat(.25)}, at(7000));
  Require(tracks[0].track_id > prior, "reset never reuses IDs");
  bool rejected = false;
  auto invalid = Cat(.2);
  invalid.box.x = std::numeric_limits<double>::quiet_NaN();
  try {
    tracker.Update({invalid}, at(8000));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  Require(rejected, "invalid measurement rejected");
  rejected = false;
  try {
    tracker.Update({}, at(7000));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  Require(rejected, "repeated timestamp rejected");
  // Class gating forbids assigning a cat's ID to an overlapping person.
  tracker.Reset();
  tracks = tracker.Update({Cat(.2)}, at(8000));
  prior = tracks[0].track_id;
  tracks = tracker.Update({{0, .9f, {.2, .2, .15, .2}}}, at(9000));
  Require(tracks.size() == 2 && tracks[0].track_id == prior && !tracks[0].observed &&
              tracks[1].class_id == 0,
          "class isolation");
  // Every detection/track can match at most once, even with overlapping boxes.
  tracker.Reset();
  tracker.Update({Cat(.2)}, at(10000));
  tracks = tracker.Update({Cat(.21), Cat(.22)}, at(11000));
  Require(tracks.size() == 2 && tracks[0].track_id != tracks[1].track_id, "one-to-one association");
  invalid = Cat(.2);
  invalid.box.width = 1e-300;
  invalid.box.height = 1e-300;
  rejected = false;
  try {
    tracker.Update({invalid}, at(12000));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  Require(rejected, "degenerate area must not produce NaN association scores");
}
