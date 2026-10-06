#ifndef COCO_PERCEPTION_PET_APPEARANCE_H_
#define COCO_PERCEPTION_PET_APPEARANCE_H_
#include <string>
#include <vector>

#include "coco/domain/pet_portrait.h"
#include "coco/media/frame.h"
namespace coco {
// Lightweight appearance baseline compatible with the Python v1 gallery.
// It is not a trained pet ReID model and does not establish permanent identity.
class PetAppearance {
 public:
  explicit PetAppearance(const std::string& gallery = "");
  void Observe(const Frame& frame, const std::vector<TrackSnapshot>& tracks);
  void Clear();
  const PetPortrait& portrait() const {
    return portrait_;
  }
  const std::string& jpeg() const {
    return jpeg_;
  }
  static std::vector<float> Embedding(const cv::Mat& crop);

 private:
  struct Sample {
    std::vector<float> embedding;
    double weight = 1;
  };
  struct Profile {
    std::string name;
    std::vector<Sample> samples;
  };
  std::vector<Profile> profiles_;
  PetPortrait portrait_;
  std::string jpeg_, candidate_;
  size_t votes_ = 0;
  Clock::time_point last_seen_{};
};
}  // namespace coco
#endif
