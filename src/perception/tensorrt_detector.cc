#include <NvInfer.h>
#include <cuda_runtime_api.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "coco/perception/detector.h"
#include "coco/perception/yolo_processing.h"
#ifdef COCO_TENSORRT
#include "tensorrt_detector.h"
#endif
namespace coco {
namespace {
class Logger : public nvinfer1::ILogger {
  void log(Severity severity, const char* message) noexcept override {
    if (severity <= Severity::kWARNING) {
      std::cerr << "[trt] " << message << '\n';
    }
  }
};
void checked(cudaError_t rc) {
  if (rc != cudaSuccess) {
    throw std::runtime_error(cudaGetErrorString(rc));
  }
}
class TensorDetector final : public Detector {
  Logger logger_;
  std::unique_ptr<nvinfer1::IRuntime> runtime_;
  std::unique_ptr<nvinfer1::ICudaEngine> engine_;
  std::unique_ptr<nvinfer1::IExecutionContext> context_;
  cudaStream_t stream_ = nullptr;
  void *input_ = nullptr, *output_ = nullptr;
  std::string input_name_, output_name_;
  int size_, candidates_ = 0;
  float threshold_;
  std::vector<float> output_host_;
  void release() {
    if (stream_) {
      cudaStreamSynchronize(stream_);
    }
    if (input_) {
      cudaFree(input_);
    }
    if (output_) {
      cudaFree(output_);
    }
    if (stream_) {
      cudaStreamDestroy(stream_);
    }
  }

 public:
  TensorDetector(const std::string& path, int size, float threshold)
      : size_(size), threshold_(threshold) {
    try {
      std::ifstream file(path, std::ios::binary);
      if (!file) {
        throw std::runtime_error("Cannot open TensorRT engine");
      }
      std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
      runtime_.reset(nvinfer1::createInferRuntime(logger_));
      if (!runtime_) {
        throw std::runtime_error("TensorRT runtime unavailable");
      }
      engine_.reset(runtime_->deserializeCudaEngine(bytes.data(), bytes.size()));
      if (!engine_) {
        throw std::runtime_error(
            "Engine deserialization failed; build a raw engine on this Jetson with trtexec");
      }
      if (engine_->getNbIOTensors() != 2) {
        throw std::runtime_error("Expected one input and one raw YOLO output");
      }
      for (int i = 0; i < 2; ++i) {
        const char* name = engine_->getIOTensorName(i);
        if (engine_->getTensorDataType(name) != nvinfer1::DataType::kFLOAT ||
            engine_->getTensorLocation(name) != nvinfer1::TensorLocation::kDEVICE ||
            engine_->getTensorFormat(name) != nvinfer1::TensorFormat::kLINEAR) {
          throw std::runtime_error("Expected linear device FP32 IO tensors");
        }
        if (engine_->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) {
          input_name_ = name;
        } else {
          output_name_ = name;
        }
      }
      if (input_name_.empty() || output_name_.empty()) {
        throw std::runtime_error("Missing input/output tensor");
      }
      auto in = engine_->getTensorShape(input_name_.c_str());
      if (in.nbDims != 4 || in.d[0] != 1 || in.d[1] != 3 || in.d[2] != size_ || in.d[3] != size_) {
        throw std::runtime_error("Engine requires fixed [1,3,imgsz,imgsz] input");
      }
      auto out = engine_->getTensorShape(output_name_.c_str());
      if (out.nbDims != 3 || out.d[0] != 1 || out.d[1] != 84 || out.d[2] <= 0) {
        throw std::runtime_error("Engine requires raw COCO YOLOv8 [1,84,N] output");
      }
      candidates_ = out.d[2];
      output_host_.resize(size_t(84) * candidates_);
      context_.reset(engine_->createExecutionContext());
      if (!context_) {
        throw std::runtime_error("Cannot create execution context");
      }
      checked(cudaStreamCreate(&stream_));
      checked(cudaMalloc(&input_, size_t(3) * size_ * size_ * sizeof(float)));
      checked(cudaMalloc(&output_, output_host_.size() * sizeof(float)));
      if (!context_->setTensorAddress(input_name_.c_str(), input_) ||
          !context_->setTensorAddress(output_name_.c_str(), output_)) {
        throw std::runtime_error("Cannot bind engine tensors");
      }
    } catch (...) {
      release();
      throw;
    }
  }
  ~TensorDetector() override {
    release();
  }
  std::vector<Detection> Infer(const Frame& f) override {
    auto input = PrepareImage(f.image, size_);
    checked(cudaMemcpyAsync(input_, input.blob.ptr<float>(), input.blob.total() * sizeof(float),
                            cudaMemcpyHostToDevice, stream_));
    if (!context_->enqueueV3(stream_)) {
      throw std::runtime_error("TensorRT inference failed");
    }
    checked(cudaMemcpyAsync(output_host_.data(), output_, output_host_.size() * sizeof(float),
                            cudaMemcpyDeviceToHost, stream_));
    checked(cudaStreamSynchronize(stream_));
    return DecodeYolo(output_host_.data(), 84, candidates_, input, f.image.size(), threshold_);
  }
};
}  // namespace
std::unique_ptr<Detector> MakeTensorRtDetector(const std::string& path, int size, float threshold) {
  return std::make_unique<TensorDetector>(path, size, threshold);
}
}  // namespace coco
