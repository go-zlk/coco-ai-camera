#ifndef COCO_APPLICATION_CONTEXT_SERVICE_H_
#define COCO_APPLICATION_CONTEXT_SERVICE_H_
#include <string>
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
// Throws on invalid configuration, model, database or inference failure.
int RunContextService(const ServiceConfig& config);
}  // namespace coco
#endif  // COCO_APPLICATION_CONTEXT_SERVICE_H_
