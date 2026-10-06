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
  Require(activity.evidence() == "held" && activity.last_seen_seconds() == 1,
          "miss has explicit evidence and age");
  context.cat_count = 1;
  observe(27, .26);
  Require(activity.state() == "resting", "short gap recovery");
  context.cat_count = 0;
  context.cat = "out_of_view";  // Presence debounce cannot bypass the activity grace window.
  context.tracks[0].observed = false;
  for (int sec = 28; sec <= 30; ++sec) {
    activity.Update(context, epoch + std::chrono::seconds(sec), "time");
    Require(activity.state() == "resting" && activity.evidence() == "held", "three second hold");
  }
  activity.Update(context, epoch + std::chrono::seconds(31), "time");
  Require(activity.state() == "unknown" && activity.evidence() == "searching", "bounded search");
  activity.Update(context, epoch + std::chrono::seconds(35), "time");
  Require(activity.evidence() == "searching", "eight second boundary");
  activity.Update(context, epoch + std::chrono::seconds(36), "time");
  Require(activity.state() == "out_of_view", "long absence stops hold");
  context.cat_count = 1;
  context.cat = "visible";
  observe(37, .26);
  Require(activity.state() == "unknown", "long gap needs new activity evidence");
  context.cat_count = 2;
  activity.Update(context, epoch + std::chrono::seconds(38), "time");
  Require(activity.state() == "unknown", "multi-cat ambiguity protected");
  Require(activity.evidence() == "ambiguous", "ambiguity is explained");
  context.cat_count = 0;
  context.cat = "out_of_view";
  activity.Update(context, epoch + std::chrono::seconds(39), "time");
  Require(activity.state() == "out_of_view", "absence differs from offline");
  context.health = "offline";
  activity.Update(context, epoch + std::chrono::seconds(40), "time");
  Require(activity.state() == "offline", "offline override");
  context.health = "online";
  context.cat = "visible";
  context.cat_count = 1;
  observe(41, .26);
  context.cat_count = 0;
  context.tracks.clear();
  context.health = "offline";
  activity.Update(context, epoch + std::chrono::seconds(42), "time");
  Require(activity.state() == "offline" && activity.evidence() == "offline", "offline beats hold");
  coco::PetActivity jittered;
  context.health = "online";
  context.cat_count = 1;
  coco::TrackSnapshot still;
  still.track_id = 1;
  still.class_id = 15;
  still.confirmed = still.observed = true;
  still.confidence = .9;
  still.box = {.2, .2, .15, .2};
  for (int sample = 0; sample < 45; ++sample) {
    auto now = epoch + std::chrono::milliseconds(sample * 511);
    still.last_seen = now;
    context.tracks = {still};
    jittered.Update(context, now, "time");
  }
  Require(jittered.state() == "resting", "irregular sample timing can confirm rest");
}
