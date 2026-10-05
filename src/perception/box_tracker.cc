#include "coco/perception/box_tracker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace coco {
namespace {
constexpr size_t kMaximumTargets = 256;
double Area(const BoundingBox& box) {
  return box.width * box.height;
}
double CenterX(const BoundingBox& box) {
  return box.x + box.width / 2;
}
double CenterY(const BoundingBox& box) {
  return box.y + box.height / 2;
}
double Iou(const BoundingBox& a, const BoundingBox& b) {
  double width = std::max(0., std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x));
  double height = std::max(0., std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y));
  double intersection = width * height;
  return intersection / (Area(a) + Area(b) - intersection);
}
void Validate(const BoxDetection& detection) {
  const auto& b = detection.box;
  if ((detection.class_id != 0 && detection.class_id != 15) ||
      !std::isfinite(detection.confidence) || detection.confidence <= 0 ||
      detection.confidence > 1 || !std::isfinite(b.x) || !std::isfinite(b.y) ||
      !std::isfinite(b.width) || !std::isfinite(b.height) || b.x < 0 || b.y < 0 || b.width <= 0 ||
      b.height <= 0 || Area(b) < 1e-12 || b.x + b.width > 1.00001 || b.y + b.height > 1.00001) {
    throw std::invalid_argument("Tracker requires valid person/cat normalized boxes");
  }
}
}  // namespace

BoxTracker::BoxTracker(TrackerConfig config) : config_(config) {
  if (!std::isfinite(config.minimum_iou) || config.minimum_iou <= 0 || config.minimum_iou > 1 ||
      !std::isfinite(config.maximum_center_distance) || config.maximum_center_distance <= 0 ||
      config.maximum_center_distance > 1 || config.lost_after.count() <= 0 ||
      config.confirmation_hits == 0) {
    throw std::invalid_argument("Invalid tracker configuration");
  }
}
void BoxTracker::Reset() {
  tracks_.clear();
  started_ = false;
}
std::vector<TrackSnapshot> BoxTracker::Update(const std::vector<BoxDetection>& detections,
                                              Clock::time_point received) {
  if (detections.size() > kMaximumTargets) {
    throw std::invalid_argument("Too many detections");
  }
  for (const auto& detection : detections) {
    Validate(detection);
  }
  if (started_ && received <= last_update_) {
    throw std::invalid_argument("Tracker timestamps must strictly increase");
  }
  tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                               [&](const Track& track) {
                                 return received - track.snapshot.last_seen >= config_.lost_after;
                               }),
                tracks_.end());
  struct Candidate {
    size_t track;
    size_t detection;
    double score;
  };
  std::vector<Candidate> candidates;
  for (size_t t = 0; t < tracks_.size(); ++t) {
    const auto& track = tracks_[t];
    double dt = std::chrono::duration<double>(received - track.snapshot.last_seen).count();
    BoundingBox predicted = track.snapshot.box;
    predicted.x =
        std::clamp(predicted.x + track.velocity_x * dt, 0., std::max(0., 1 - predicted.width));
    predicted.y =
        std::clamp(predicted.y + track.velocity_y * dt, 0., std::max(0., 1 - predicted.height));
    for (size_t d = 0; d < detections.size(); ++d) {
      const auto& detection = detections[d];
      if (detection.class_id != track.snapshot.class_id) {
        continue;
      }
      double ratio = Area(detection.box) / Area(predicted);
      if (ratio < .4 || ratio > 2.5) {
        continue;
      }
      double overlap = Iou(predicted, detection.box);
      double distance = std::hypot(CenterX(predicted) - CenterX(detection.box),
                                   CenterY(predicted) - CenterY(detection.box));
      if (overlap < config_.minimum_iou && distance > config_.maximum_center_distance) {
        continue;
      }
      candidates.push_back({t, d, overlap - distance * .1});
    }
  }
  std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
    if (a.score != b.score) {
      return a.score > b.score;
    }
    if (a.track != b.track) {
      return a.track < b.track;
    }
    return a.detection < b.detection;
  });
  std::vector<bool> matched_tracks(tracks_.size(), false),
      matched_detections(detections.size(), false);
  for (const auto& candidate : candidates) {
    if (matched_tracks[candidate.track] || matched_detections[candidate.detection]) {
      continue;
    }
    auto& track = tracks_[candidate.track];
    const auto& detection = detections[candidate.detection];
    double dt = std::chrono::duration<double>(received - track.snapshot.last_seen).count();
    // A bounded predictor guides association only; the API retains real measured boxes.
    track.velocity_x =
        std::clamp((CenterX(detection.box) - CenterX(track.snapshot.box)) / dt, -.5, .5);
    track.velocity_y =
        std::clamp((CenterY(detection.box) - CenterY(track.snapshot.box)) / dt, -.5, .5);
    track.snapshot.box = detection.box;
    track.snapshot.confidence = detection.confidence;
    track.snapshot.last_seen = received;
    track.snapshot.observed = true;
    ++track.hits;
    track.snapshot.confirmed = track.hits >= config_.confirmation_hits;
    track.snapshot.status = track.snapshot.confirmed ? "confirmed" : "tentative";
    matched_tracks[candidate.track] = true;
    matched_detections[candidate.detection] = true;
  }
  for (size_t t = 0; t < tracks_.size(); ++t) {
    if (!matched_tracks[t]) {
      tracks_[t].snapshot.observed = false;
      tracks_[t].snapshot.status = "lost";
    }
  }
  for (size_t d = 0; d < detections.size(); ++d) {
    if (matched_detections[d]) {
      continue;
    }
    if (tracks_.size() >= kMaximumTargets || next_id_ == std::numeric_limits<uint64_t>::max()) {
      throw std::runtime_error("Tracker capacity exhausted");
    }
    Track track;
    track.snapshot.track_id = next_id_++;
    track.snapshot.class_id = detections[d].class_id;
    track.snapshot.confidence = detections[d].confidence;
    track.snapshot.box = detections[d].box;
    track.snapshot.last_seen = received;
    track.snapshot.observed = true;
    track.snapshot.confirmed = config_.confirmation_hits == 1;
    track.snapshot.status = track.snapshot.confirmed ? "confirmed" : "tentative";
    tracks_.push_back(track);
  }
  last_update_ = received;
  started_ = true;
  std::vector<TrackSnapshot> result;
  for (const auto& track : tracks_) {
    result.push_back(track.snapshot);
  }
  return result;
}
}  // namespace coco
