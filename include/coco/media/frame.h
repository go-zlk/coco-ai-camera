#ifndef COCO_MEDIA_FRAME_H_
#define COCO_MEDIA_FRAME_H_
#include <cstdint>
#include <opencv2/core.hpp>
#include <string>

#include "coco/domain/context.h"
namespace coco {
struct Frame {
  cv::Mat image;
  uint64_t sequence = 0;
  Clock::time_point received;
  std::string observed_at;
};
struct Detection {
  int class_id;
  float confidence;
  cv::Rect2f box;
};
}  // namespace coco
#endif  // COCO_MEDIA_FRAME_H_
