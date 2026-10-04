#include "coco/perception/detector.h"

#include <algorithm>
#include <cmath>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

#include "coco/perception/yolo_processing.h"
#ifdef COCO_TENSORRT
#include "tensorrt_detector.h"
#endif
namespace coco {
PreparedImage PrepareImage(const cv::Mat& image, int size) {
  float scale = std::min(float(size) / image.cols, float(size) / image.rows);
  int w = std::lround(image.cols * scale), h = std::lround(image.rows * scale);
  int left = (size - w) / 2, top = (size - h) / 2;
  cv::Mat resized, padded;
  cv::resize(image, resized, {w, h});
  cv::copyMakeBorder(resized, padded, top, size - h - top, left, size - w - left,
                     cv::BORDER_CONSTANT, {114, 114, 114});
  return {cv::dnn::blobFromImage(padded, 1. / 255., {size, size}, {}, true, false), scale, left,
          top};
}
std::vector<Detection> DecodeYolo(const float* p, int features, int candidates,
                                  const PreparedImage& prep, cv::Size original, float threshold) {
  if (features != 84 || candidates <= 0) {
    throw std::runtime_error("Expected raw COCO YOLOv8 output [1,84,N], without embedded NMS");
  }
  std::vector<Detection> result;
  for (int cls : {0, 15}) {
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    for (int i = 0; i < candidates; ++i) {
      int best_class = 0;
      for (int k = 1; k < 80; ++k) {
        if (p[(4 + k) * candidates + i] > p[(4 + best_class) * candidates + i]) {
          best_class = k;
        }
      }
      if (best_class != cls) {
        continue;
      }
      float score = p[(4 + cls) * candidates + i];
      if (!std::isfinite(score) || score < threshold) {
        continue;
      }
      float cx = p[i], cy = p[candidates + i], w = p[2 * candidates + i], h = p[3 * candidates + i];
      if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(w) || !std::isfinite(h) ||
          w <= 0 || h <= 0) {
        continue;
      }
      int x = std::lround((cx - w / 2 - prep.left) / prep.scale),
          y = std::lround((cy - h / 2 - prep.top) / prep.scale);
      cv::Rect box(x, y, std::lround(w / prep.scale), std::lround(h / prep.scale));
      box &= cv::Rect(0, 0, original.width, original.height);
      if (box.area() <= 0) {
        continue;
      }
      boxes.push_back(box);
      scores.push_back(score);
    }
    std::vector<int> kept;
    cv::dnn::NMSBoxes(boxes, scores, threshold, 0.45f, kept);
    for (int i : kept) {
      result.push_back({cls, scores[i], cv::Rect2f(boxes[i])});
    }
  }
  return result;
}
class OnnxDetector final : public Detector {
  cv::dnn::Net net_;
  int size_;
  float threshold_;

 public:
  OnnxDetector(const std::string& path, int size, float threshold)
      : net_(cv::dnn::readNetFromONNX(path)), size_(size), threshold_(threshold) {}
  std::vector<Detection> Infer(const Frame& f) override {
    auto input = PrepareImage(f.image, size_);
    net_.setInput(input.blob);
    auto output = net_.forward();
    if (output.dims != 3 || output.size[0] != 1) {
      throw std::runtime_error("Expected batch-one YOLOv8 output");
    }
    return DecodeYolo(output.ptr<float>(), output.size[1], output.size[2], input, f.image.size(),
                      threshold_);
  }
};
std::unique_ptr<Detector> MakeDetector(const std::string& backend, const std::string& model,
                                       int size, float threshold) {
  if (backend == "onnx") {
    return std::make_unique<OnnxDetector>(model, size, threshold);
  }
#ifdef COCO_TENSORRT
  if (backend == "tensorrt") {
    return MakeTensorRtDetector(model, size, threshold);
  }
#endif
  throw std::runtime_error("Unsupported backend or TensorRT disabled at build time");
}
}  // namespace coco
