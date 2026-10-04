#include "coco/capture/camera_source.h"

#include <algorithm>
#include <iostream>
#include <opencv2/videoio.hpp>
#include <utility>
namespace coco {
CameraSource::CameraSource(std::string uri) : uri_(std::move(uri)) {}
CameraSource::~CameraSource() {
  Stop();
}
void CameraSource::Start() {
  if (!running_.exchange(true)) {
    worker_ = std::thread(&CameraSource::Capture, this);
  }
}
void CameraSource::Stop() {
  running_ = false;
  cv_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
}
std::optional<Frame> CameraSource::Take(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait_for(lock, timeout, [&] { return pending_.has_value() || !running_; });
  auto value = std::move(pending_);
  pending_.reset();
  return value;
}
void CameraSource::Capture() {
  uint64_t sequence = 0;
  while (running_) {
    try {
      cv::VideoCapture camera;
      if (uri_.rfind("rtsp://", 0) == 0 || uri_.rfind("rtsps://", 0) == 0) {
        camera.open(uri_, cv::CAP_FFMPEG,
                    {cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000, cv::CAP_PROP_READ_TIMEOUT_MSEC, 3000});
      } else if (uri_ == "csi") {
        camera.open(
            "nvarguscamerasrc ! "
            "video/x-raw(memory:NVMM),width=1280,height=720,format=NV12,framerate=30/1 ! nvvidconv "
            "! video/x-raw,format=BGRx ! videoconvert ! video/x-raw,format=BGR ! appsink "
            "max-buffers=1 drop=true sync=false",
            cv::CAP_GSTREAMER);
      } else if (!uri_.empty() && std::all_of(uri_.begin(), uri_.end(), [](unsigned char c) {
                   return c >= '0' && c <= '9';
                 })) {
        camera.open(std::stoi(uri_));
      } else {
        camera.open(uri_);
      }
      while (running_ && camera.isOpened()) {
        Frame f;
        if (!camera.read(f.image) || f.image.empty()) {
          break;
        }
        f.sequence = ++sequence;
        f.received = Clock::now();
        f.observed_at = UtcNow();
        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (pending_) {
            ++dropped_;
          }
          pending_ = std::move(f);
        }
        cv_.notify_one();
      }
      camera.release();
    } catch (const std::exception&) {
      std::cerr << "[capture] source unavailable; reconnecting\n";
    }
    // Interruptible retry, without logging source credentials.
    for (int i = 0; i < 20 && running_; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}
}  // namespace coco
