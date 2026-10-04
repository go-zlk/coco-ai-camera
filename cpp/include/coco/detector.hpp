#pragma once
#include "coco/types.hpp"
#include <memory>
namespace coco {
class Detector {
public:
  virtual ~Detector() = default;
  virtual std::vector<Detection> infer(const Frame& frame) = 0;
};
struct PreparedImage { cv::Mat blob; float scale; int left, top; };
PreparedImage prepare(const cv::Mat& image, int size);
std::vector<Detection> decode_yolo(const float* values, int features, int candidates,
                                  const PreparedImage& prep, cv::Size original, float threshold);
std::unique_ptr<Detector> make_detector(const std::string& backend, const std::string& model, int size, float threshold);
#ifdef COCO_TENSORRT
std::unique_ptr<Detector> make_tensorrt_detector(const std::string& model, int size, float threshold);
#endif
}
