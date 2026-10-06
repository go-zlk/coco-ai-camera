#include "coco/domain/pet_activity.h"

#include <algorithm>
#include <chrono>
#include <cmath>
namespace coco {
std::optional<Event> PetActivity::Update(const Context& context, Clock::time_point now,
                                         const std::string& observed_at) {
  std::string desired = "unknown";
  float confidence = 0;
  evidence_ = "none";
  last_seen_seconds_ =
      last_confirmed_ ? std::chrono::duration<double>(now - *last_confirmed_).count() : -1;
  const TrackSnapshot* cat = nullptr;
  size_t observed_cats = 0;
  for (const auto& track : context.tracks) {
    if (track.class_id == 15 && track.observed) {
      ++observed_cats;
      cat = &track;
    }
  }
  if (context.health == "offline") {
    desired = "offline";
    evidence_ = "offline";
    last_confirmed_.reset();
    last_seen_seconds_ = -1;
  } else if (context.cat_count > 1 || observed_cats > 1) {
    evidence_ = "ambiguous";
    last_confirmed_.reset();
    last_seen_seconds_ = -1;
    samples_.clear();
    moving_since_.reset();
    track_id_ = 0;
  } else if (context.cat_count == 1 && observed_cats == 1 && cat->confirmed) {
    evidence_ = "observed";
    last_confirmed_ = now;
    last_seen_seconds_ = 0;
    if (track_id_ != cat->track_id ||
        (!samples_.empty() && now - samples_.back().at > std::chrono::seconds(3))) {
      samples_.clear();
      moving_since_.reset();
    }
    track_id_ = cat->track_id;
    bool moving = false;
    if (!samples_.empty()) {
      const auto& previous = samples_.back();
      double dt = std::chrono::duration<double>(now - previous.at).count();
      if (dt > 0) {
        double dx = cat->box.x + cat->box.width / 2 - previous.box.x - previous.box.width / 2;
        double dy = cat->box.y + cat->box.height / 2 - previous.box.y - previous.box.height / 2;
        double area = cat->box.width * cat->box.height;
        double previous_area = previous.box.width * previous.box.height;
        moving =
            std::hypot(dx, dy) / dt > .03 || std::abs(std::log(area / previous_area)) / dt > .12;
      }
    }
    if (moving) {
      if (!moving_since_) {
        moving_since_ = now;
      }
    } else {
      moving_since_.reset();
    }
    samples_.push_back({now, cat->box});
    // Keep the sample at/before the window boundary. Requiring a sample exactly
    // twenty seconds old would never confirm rest with irregular inference timing.
    while (samples_.size() > 1 && now - samples_[1].at >= std::chrono::seconds(20)) {
      samples_.pop_front();
    }
    bool resting = samples_.size() >= 10 && now - samples_.front().at >= std::chrono::seconds(20);
    if (resting) {
      double min_x = 1, min_y = 1, max_x = 0, max_y = 0, min_area = 1, max_area = 0;
      for (const auto& sample : samples_) {
        min_x = std::min(min_x, sample.box.x + sample.box.width / 2);
        max_x = std::max(max_x, sample.box.x + sample.box.width / 2);
        min_y = std::min(min_y, sample.box.y + sample.box.height / 2);
        max_y = std::max(max_y, sample.box.y + sample.box.height / 2);
        double area = sample.box.width * sample.box.height;
        min_area = std::min(min_area, area);
        max_area = std::max(max_area, area);
      }
      resting = std::hypot(max_x - min_x, max_y - min_y) <= .03 && max_area / min_area <= 1.25;
    }
    if (moving_since_ && now - *moving_since_ >= std::chrono::seconds(2)) {
      desired = "active";
    } else if (resting) {
      desired = "resting";
    } else if (state_ == "active" && now - changed_ < std::chrono::seconds(5)) {
      desired = "active";
    }
    if (desired == "active" || desired == "resting") {
      confidence = cat->confidence;
    }
  } else {
    if (last_confirmed_ && last_seen_seconds_ <= 3) {
      // Hold only a recent decision. Counts and boxes continue to report actual detections.
      evidence_ = "held";
      desired = state_;
      confidence = confidence_;
    } else if (last_confirmed_ && last_seen_seconds_ <= 8) {
      evidence_ = "searching";
    } else if (context.cat_count == 0 && context.cat == "out_of_view") {
      desired = "out_of_view";
      evidence_ = "absent";
    } else {
      evidence_ = context.cat_count ? "confirming" : "none";
    }
    if (evidence_ != "held") {
      samples_.clear();
      moving_since_.reset();
      track_id_ = 0;
    }
  }
  if (desired == "offline" || desired == "out_of_view") {
    samples_.clear();
    moving_since_.reset();
    track_id_ = 0;
  }
  confidence_ = confidence;
  if (desired == state_) {
    return {};
  }
  state_ = desired;
  changed_ = now;
  return Event{context.source_id, "pet", state_, observed_at, confidence_};
}
}  // namespace coco
