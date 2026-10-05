#ifndef COCO_APPLICATION_POC_SERVICE_H_
#define COCO_APPLICATION_POC_SERVICE_H_
#include <functional>
#include <string>

#include "coco/application/replay_service.h"
namespace coco {
struct PocConfig {
  ReplayConfig replay;
  int port = 8091;
  std::string web_root = "web";
};
int RunPocService(const PocConfig& config, const std::function<bool()>& stop_requested);
}  // namespace coco
#endif
