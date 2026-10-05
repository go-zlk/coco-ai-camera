#ifndef COCO_APPLICATION_CONTEXT_SERVICE_H_
#define COCO_APPLICATION_CONTEXT_SERVICE_H_

#include <functional>

#include "coco/application/service_config.h"

namespace coco {
// Blocking single-source service. The caller owns shutdown policy. stop_requested
// is polled on the application thread and must be nonblocking; returns 2 if no inference ran.
// Throws on invalid configuration, model, database or inference failure.
int RunContextService(
    const ServiceConfig& config,
    const std::function<bool()>& stop_requested = [] { return false; });
}  // namespace coco
#endif  // COCO_APPLICATION_CONTEXT_SERVICE_H_
