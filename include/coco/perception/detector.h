#ifndef COCO_PERCEPTION_DETECTOR_H_
#define COCO_PERCEPTION_DETECTOR_H_
#include <memory>
#include <string>
#include <vector>

#include "coco/media/frame.h"
namespace coco {
class Detector {
 public:
  virtual ~Detector() = default;
  virtual std::vector<Detection> Infer(const Frame& frame) = 0;
};
std::unique_ptr<Detector> MakeDetector(const std::string& backend, const std::string& model,
                                       int size, float threshold);

}  // namespace coco

#endif  // COCO_PERCEPTION_DETECTOR_H_
