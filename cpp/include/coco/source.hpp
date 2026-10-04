#pragma once
#include "coco/types.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
namespace coco {
// Single-slot queue. Old pending frames are dropped to bound memory and latency.
class CameraSource {
public:
  explicit CameraSource(std::string uri);
  ~CameraSource();
  void start();
  void stop();
  std::optional<Frame> take(std::chrono::milliseconds timeout);
  uint64_t dropped() const { return dropped_; }
private:
  void capture();
  std::string uri_;
  std::atomic<bool> running_{false};
  std::atomic<uint64_t> dropped_{0};
  std::thread worker_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<Frame> pending_;
};
}
