#ifndef COCO_PERCEPTION_TENSORRT_DETECTOR_H_
#define COCO_PERCEPTION_TENSORRT_DETECTOR_H_
#include "coco/perception/detector.h"
namespace coco {
std::unique_ptr<Detector> MakeTensorRtDetector(const std::string& model, int size, float threshold);
}
#endif  // COCO_PERCEPTION_TENSORRT_DETECTOR_H_
