#include "coco/perception/pet_appearance.h"

#include <algorithm>
#include <cmath>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
namespace coco {
namespace {
constexpr size_t kDimensions = 166;
double Cosine(const std::vector<float>& a, const std::vector<float>& b) {
  double dot = 0, aa = 0, bb = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    aa += a[i] * a[i];
    bb += b[i] * b[i];
  }
  return dot / (std::sqrt(aa * bb) + 1e-8);
}
}  // namespace
PetAppearance::PetAppearance(const std::string& gallery) {
  if (gallery.empty()) {
    return;
  }
  cv::FileStorage file(gallery, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  if (!file.isOpened() || int(file["version"]) != 1 || !file["profiles"].isSeq()) {
    throw std::invalid_argument("Invalid appearance gallery v1");
  }
  for (const auto& node : file["profiles"]) {
    Profile profile;
    node["identity"] >> profile.name;
    if (profile.name.empty() || profile.name.size() > 128 || !node["samples"].isSeq()) {
      throw std::invalid_argument("Invalid gallery profile");
    }
    for (const auto& item : node["samples"]) {
      Sample sample;
      item["embedding"] >> sample.embedding;
      if (!item["weight"].empty()) {
        sample.weight = double(item["weight"]);
      }
      if (sample.embedding.size() != kDimensions || !std::isfinite(sample.weight) ||
          sample.weight <= 0 || sample.weight > 1 ||
          !std::all_of(sample.embedding.begin(), sample.embedding.end(),
                       [](float f) { return std::isfinite(f); }) ||
          Cosine(sample.embedding, sample.embedding) < .99) {
        throw std::invalid_argument("Invalid gallery embedding");
      }
      profile.samples.push_back(std::move(sample));
    }
    if (profile.samples.empty()) {
      throw std::invalid_argument("Empty gallery profile");
    }
    if (std::any_of(profiles_.begin(), profiles_.end(),
                    [&](const Profile& p) { return p.name == profile.name; })) {
      throw std::invalid_argument("Duplicate gallery identity");
    }
    profiles_.push_back(std::move(profile));
  }
}
std::vector<float> PetAppearance::Embedding(const cv::Mat& crop) {
  if (crop.empty() || crop.type() != CV_8UC3) {
    throw std::invalid_argument("Expected BGR crop");
  }
  cv::Mat hsv, gray, small;
  cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);
  int channels[] = {0, 1}, bins[] = {24, 4};
  float hue[] = {0, 180}, saturation[] = {0, 256};
  const float* ranges[] = {hue, saturation};
  cv::Mat histogram;
  cv::calcHist(&hsv, 1, channels, cv::Mat(), histogram, 2, bins, ranges);
  histogram /= cv::norm(histogram) + 1e-8;
  std::vector<float> result(histogram.ptr<float>(), histogram.ptr<float>() + 96);
  cv::Scalar mean, deviation;
  cv::meanStdDev(hsv, mean, deviation);
  for (int i = 0; i < 3; ++i) {
    result.push_back(mean[i] / 255.);
  }
  for (int i = 0; i < 3; ++i) {
    result.push_back(deviation[i] / 255.);
  }
  cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
  cv::resize(gray, small, {8, 8}, 0, 0, cv::INTER_AREA);
  cv::meanStdDev(small, mean, deviation);
  for (int i = 0; i < 64; ++i) {
    result.push_back(.25 * (small.data[i] - mean[0]) / (deviation[0] + 1e-6));
  }
  double norm = 0;
  for (auto value : result) {
    norm += value * value;
  }
  norm = std::sqrt(norm) + 1e-8;
  for (auto& value : result) {
    value /= norm;
  }
  return result;
}
void PetAppearance::Clear() {
  portrait_ = {};
  jpeg_.clear();
  candidate_.clear();
  votes_ = 0;
  last_seen_ = {};
}
void PetAppearance::Observe(const Frame& frame, const std::vector<TrackSnapshot>& tracks) {
  const TrackSnapshot* cat = nullptr;
  size_t count = 0;
  for (const auto& track : tracks) {
    if (track.class_id == 15 && track.observed) {
      cat = &track;
      ++count;
    }
  }
  if (!count) {
    if (last_seen_ != Clock::time_point{} &&
        frame.received - last_seen_ <= std::chrono::seconds(3)) {
      portrait_.status = "held";
    } else {
      Clear();
    }
    return;
  }
  if (count != 1) {
    Clear();
    portrait_.status = "ambiguous";
    return;
  }
  if (cat->track_id != portrait_.track_id ||
      (last_seen_ != Clock::time_point{} &&
       frame.received - last_seen_ > std::chrono::seconds(3))) {
    Clear();
  }
  portrait_.track_id = cat->track_id;
  portrait_.sequence = frame.sequence;
  portrait_.observed_at = frame.observed_at;
  portrait_.name.clear();
  portrait_.similarity = 0;
  last_seen_ = frame.received;
  const auto& b = cat->box;
  const cv::Rect bounds(0, 0, frame.image.cols, frame.image.rows);
  const cv::Rect box(int(b.x * bounds.width), int(b.y * bounds.height), int(b.width * bounds.width),
                     int(b.height * bounds.height));
  const auto clipped = box & bounds;
  if (clipped.width < 48 || clipped.height < 48) {
    jpeg_.clear();
    portrait_.thumbnail_available = false;
    portrait_.status = "low_quality";
    candidate_.clear();
    votes_ = 0;
    return;
  }
  const auto crop = frame.image(clipped);
  cv::Mat thumbnail;
  double scale = std::min(1., 320. / std::max(crop.cols, crop.rows));
  cv::resize(crop, thumbnail, {}, scale, scale, cv::INTER_AREA);
  std::vector<unsigned char> encoded;
  if (!cv::imencode(".jpg", thumbnail, encoded, {cv::IMWRITE_JPEG_QUALITY, 80})) {
    throw std::runtime_error("Thumbnail encoding failed");
  }
  jpeg_.assign(encoded.begin(), encoded.end());
  portrait_.thumbnail_available = true;
  portrait_.status = profiles_.empty() ? "unregistered" : "confirming";
  if (!cat->confirmed || profiles_.empty()) {
    candidate_.clear();
    votes_ = 0;
    return;
  }
  cv::Mat gray, laplacian;
  cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
  cv::Laplacian(gray, laplacian, CV_64F);
  cv::Scalar mean, deviation;
  cv::meanStdDev(laplacian, mean, deviation);
  if (deviation[0] * deviation[0] < 20) {
    portrait_.status = "low_quality";
    candidate_.clear();
    votes_ = 0;
    return;
  }
  const auto embedding = Embedding(crop);
  double best = -1, second = -1;
  std::string name;
  for (const auto& profile : profiles_) {
    double score = -1;
    for (const auto& sample : profile.samples) {
      score = std::max(score, Cosine(embedding, sample.embedding) - (1 - sample.weight) * .05);
    }
    if (score > best) {
      second = best;
      best = score;
      name = profile.name;
    } else {
      second = std::max(second, score);
    }
  }
  portrait_.similarity = best;
  if (best < .75 || best - second < .08) {
    portrait_.status = "uncertain";
    candidate_.clear();
    votes_ = 0;
    return;
  }
  if (candidate_ == name) {
    ++votes_;
  } else {
    candidate_ = name;
    votes_ = 1;
  }
  if (votes_ >= 5) {
    portrait_.name = name;
    portrait_.status = "matched";
  }
}
}  // namespace coco
