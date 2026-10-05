#ifndef COCO_APPLICATION_SERVICE_CONFIG_H_
#define COCO_APPLICATION_SERVICE_CONFIG_H_

#include <string>
#include <vector>

namespace coco {
struct ServiceConfig {
  std::string source;
  std::string model;
  std::string source_id = "camera_1";
  std::string backend = "tensorrt";
  std::string database = "data/context.db";
  int port = 8090;
  int image_size = 640;
  int interval_ms = 1000;
  float confidence = 0.25f;
  double duration_seconds = 0;
};
struct ParsedServiceConfig {
  ServiceConfig config;
  bool help = false;
};
void ValidateServiceConfig(const ServiceConfig& config);
// Accepts arguments without argv[0]; rejects unknown/duplicate flags and partial numbers.
ParsedServiceConfig ParseServiceConfig(const std::vector<std::string>& arguments);
std::string ServiceUsage();
}  // namespace coco
#endif  // COCO_APPLICATION_SERVICE_CONFIG_H_
