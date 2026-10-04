#include "coco/perception/yolo_processing.h"

#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void Require(bool ok) {
  if (!ok) {
    throw std::runtime_error("YOLO processing check failed");
  }
}
}  // namespace
int main() {
  // Overlapping person boxes must suppress within class, but not suppress a cat.
  std::vector<float> tensor(84 * 3, 0);
  for (int i = 0; i < 3; ++i) {
    tensor[i] = 320;
    tensor[3 + i] = 320;
    tensor[6 + i] = 100;
    tensor[9 + i] = 100;
  }
  tensor[4 * 3] = .9f;
  tensor[4 * 3 + 1] = .8f;
  tensor[(4 + 15) * 3 + 2] = .7f;
  auto prep = coco::PrepareImage(cv::Mat(480, 848, CV_8UC3, cv::Scalar(0, 0, 0)), 640);
  auto boxes = coco::DecodeYolo(tensor.data(), 84, 3, prep, {848, 480}, .25f);
  Require(boxes.size() == 2 && boxes[0].class_id == 0 && boxes[1].class_id == 15);
  Require(boxes[0].box.x >= 0 && boxes[0].box.y >= 0);
  // A stronger chair class must not become a cat merely through filtering.
  tensor[(4 + 56) * 3 + 2] = .95f;
  boxes = coco::DecodeYolo(tensor.data(), 84, 3, prep, {848, 480}, .25f);
  Require(boxes.size() == 1 && boxes[0].class_id == 0);
  std::cout << "YOLO class filtering, coordinate restoration and NMS checks passed\n";
}
