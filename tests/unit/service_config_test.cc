#include "coco/application/service_config.h"

#include <stdexcept>
#include <string>
#include <vector>
namespace {
void Require(bool condition) {
  if (!condition) {
    throw std::runtime_error("Configuration check failed");
  }
}
void Reject(std::vector<std::string> arguments) {
  bool threw = false;
  try {
    coco::ParseServiceConfig(arguments);
  } catch (const std::exception&) {
    threw = true;
  }
  Require(threw);
}
}  // namespace
int main() {
  Require(coco::ParseServiceConfig({"--help"}).help);
  auto config =
      coco::ParseServiceConfig({"--source", "csi", "--model", "model.onnx", "--backend", "onnx"})
          .config;
  Require(config.port == 8090 && config.source_id == "camera_1");
  Reject({});
  Reject({"--source", "csi", "--model", "model", "--port", "8090oops"});
  Reject({"--source", "csi", "--model", "model", "--conf", "nan"});
  Reject({"--source", "csi", "--model", "model", "--seconds", "inf"});
  Reject({"--source", "csi", "--model", "model", "--port", "65536"});
  Reject({"--source", "csi", "--model", "model", "--backend", "fake"});
  Reject({"--source", "csi", "--source", "0", "--model", "model"});
  Reject({"--source", "--model", "model"});
  Reject({"--source", "csi", "--model", "model", "--source-id", ""});
}
