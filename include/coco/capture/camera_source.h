#ifndef COCO_CAPTURE_CAMERA_SOURCE_H_
#define COCO_CAPTURE_CAMERA_SOURCE_H_
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#include "coco/media/frame.h"
namespace coco {
// Single-slot queue. Old pending frames are dropped to bound memory and latency.
class CameraSource {
 public:
  explicit CameraSource(std::string uri);
  ~CameraSource();
  void Start();
  void Stop();
  std::optional<Frame> Take(std::chrono::milliseconds timeout);
  uint64_t dropped() const {
    return dropped_;
  }

 private:
  void Capture();
  std::string uri_;
  std::atomic<bool> running_{false};
  std::atomic<uint64_t> dropped_{0};
  std::thread worker_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<Frame> pending_;
};
}  // namespace coco

#endif  // COCO_CAPTURE_CAMERA_SOURCE_H_
