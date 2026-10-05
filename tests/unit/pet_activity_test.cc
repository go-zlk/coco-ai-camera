#include "coco/domain/pet_activity.h"

#include <chrono>
#include <stdexcept>
namespace {
void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}
}  // namespace
int main() {
  coco::PetActivity activity;
  coco::Context context;
  context.source_id = "test";
  context.health = "online";
  context.cat = "visible";
  context.cat_count = 1;
  auto epoch = coco::Clock::time_point(std::chrono::seconds(1));
  auto observe = [&](int sec, double x) {
    coco::TrackSnapshot track;
    track.track_id = 1;
    track.class_id = 15;
    track.confirmed = true;
    track.observed = true;
    track.confidence = .9;
    track.box = {x, .2, .15, .2};
    track.last_seen = epoch + std::chrono::seconds(sec);
    context.tracks = {track};
    return activity.Update(context, track.last_seen, "time");
  };
  for (int sec = 0; sec <= 4; ++sec) {
    observe(sec, .1 + .04 * sec);
  }
  Require(activity.state() == "active", "movement confirmation");
  for (int sec = 5; sec <= 25; ++sec) {
    observe(sec, .26);
  }
  Require(activity.state() == "resting", "continuous stationary window");
  context.cat_count = 0;
  context.tracks[0].observed = false;
  activity.Update(context, epoch + std::chrono::seconds(26), "time");
  Require(activity.state() == "resting", "one missed frame must not flip state");
  context.cat_count = 1;
  observe(27, .26);
  Require(activity.state() == "resting", "short gap recovery");
  context.cat_count = 2;
  activity.Update(context, epoch + std::chrono::seconds(28), "time");
  Require(activity.state() == "unknown", "multi-cat ambiguity protected");
  context.cat_count = 0;
  context.cat = "out_of_view";
  activity.Update(context, epoch + std::chrono::seconds(29), "time");
  Require(activity.state() == "out_of_view", "absence differs from offline");
  context.health = "offline";
  activity.Update(context, epoch + std::chrono::seconds(30), "time");
  Require(activity.state() == "offline", "offline override");
}
