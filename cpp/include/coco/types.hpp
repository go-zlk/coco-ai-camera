#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

namespace coco {
using Clock = std::chrono::steady_clock;
struct Frame {
  cv::Mat image;
  uint64_t sequence = 0;
  Clock::time_point received;
  std::string observed_at;
};
struct Detection { int class_id; float confidence; cv::Rect2f box; };
struct Event {
  std::string source_id, category, state, observed_at;
  float confidence = 0;
};
struct Context {
  std::string source_id, observed_at, health = "starting";
  std::string person = "unknown", cat = "unknown";
  uint64_t sequence = 0, dropped_frames = 0;
  size_t person_count = 0, cat_count = 0;
  double inference_ms = 0;
  Clock::time_point updated{};
};
std::string utc_now();
std::string json_string(const std::string& value);
std::string context_json(const Context& context);
}
