#include "coco/perception/pet_appearance.h"

#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
namespace {
void Require(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}
void Gallery(const std::string& path, const std::vector<float>& embedding, bool duplicate) {
  cv::FileStorage file(path, cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  file << "version" << 1 << "profiles" << "[";
  for (const auto* name : {"kui", "coco"}) {
    if (std::string(name) == "coco" && !duplicate) {
      break;
    }
    file << "{" << "identity" << name << "samples" << "[" << "{" << "embedding" << embedding
         << "weight" << 1. << "}" << "]" << "}";
  }
  file << "]";
}
}  // namespace
int main() {
  auto path = std::filesystem::temp_directory_path() /
              ("coco-appearance-" + std::to_string(coco::Clock::now().time_since_epoch().count()) +
               ".json");
  coco::Frame frame;
  frame.image = cv::Mat(400, 600, CV_8UC3);
  cv::RNG rng(1234);
  rng.fill(frame.image, cv::RNG::UNIFORM, 0, 256);
  frame.received = coco::Clock::time_point(std::chrono::seconds(1));
  frame.observed_at = "2026-01-01T00:00:00Z";
  coco::TrackSnapshot cat;
  cat.track_id = 1;
  cat.class_id = 15;
  cat.observed = cat.confirmed = true;
  cat.box = {0, 0, 1, 1};
  coco::PetAppearance unregistered;
  unregistered.Observe(frame, {cat});
  Require(unregistered.portrait().status == "unregistered", "no gallery never invents name");
  auto bytes = unregistered.jpeg();
  auto image =
      cv::imdecode(std::vector<unsigned char>(bytes.begin(), bytes.end()), cv::IMREAD_COLOR);
  Require(!image.empty() && image.cols == 320 && image.rows <= 320, "bounded real thumbnail");
  auto embedding = coco::PetAppearance::Embedding(frame.image);
  Require(embedding.size() == 166, "Python v1 feature dimension");
  Gallery(path.string(), embedding, false);
  coco::PetAppearance matcher(path.string());
  for (int sample = 0; sample < 5; ++sample) {
    frame.sequence = sample + 1;
    frame.received += std::chrono::seconds(1);
    matcher.Observe(frame, {cat});
    Require(sample == 4 || matcher.portrait().name.empty(), "no single-frame identity claim");
  }
  Require(matcher.portrait().name == "kui" && matcher.portrait().status == "matched",
          "stable match");
  frame.received += std::chrono::seconds(1);
  matcher.Observe(frame, {});
  Require(matcher.portrait().status == "held" && matcher.portrait().name == "kui",
          "short crop hold");
  frame.received += std::chrono::seconds(3);
  matcher.Observe(frame, {});
  Require(matcher.jpeg().empty() && matcher.portrait().name.empty(), "expired crop cleared");
  matcher.Observe(frame, {cat, cat});
  Require(matcher.portrait().status == "ambiguous" && matcher.jpeg().empty(),
          "multi-cat protected");
  cat.confirmed = false;
  matcher.Observe(frame, {cat});
  Require(matcher.portrait().status == "confirming", "tentative track has no name");
  Gallery(path.string(), embedding, true);
  coco::PetAppearance ambiguous(path.string());
  cat.confirmed = true;
  for (int sample = 0; sample < 8; ++sample) {
    ambiguous.Observe(frame, {cat});
  }
  Require(ambiguous.portrait().status == "uncertain" && ambiguous.portrait().name.empty(),
          "similar profiles cannot win by order");
  frame.image.setTo(cv::Scalar(100, 100, 100));
  matcher.Observe(frame, {cat});
  Require(matcher.portrait().status == "low_quality" && matcher.portrait().name.empty(),
          "blur rejects identity");
  matcher.Clear();
  Require(matcher.jpeg().empty(), "offline clear removes media");
  std::filesystem::remove(path);
}
