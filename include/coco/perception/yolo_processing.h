#ifndef COCO_PERCEPTION_YOLO_PROCESSING_H_
#define COCO_PERCEPTION_YOLO_PROCESSING_H_
#include <vector>

#include "coco/media/frame.h"
namespace coco {
struct PreparedImage {
  cv::Mat blob;
  float scale;
  int left, top;
};
PreparedImage PrepareImage(const cv::Mat& image, int size);
std::vector<Detection> DecodeYolo(const float* values, int features, int candidates,
                                  const PreparedImage& prep, cv::Size original, float threshold);
}  // namespace coco
#endif  // COCO_PERCEPTION_YOLO_PROCESSING_H_
