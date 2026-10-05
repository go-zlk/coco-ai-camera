#ifndef COCO_APPLICATION_DASHBOARD_H_
#define COCO_APPLICATION_DASHBOARD_H_
#include <string>

#include "coco/domain/context.h"
#include "coco/storage/event_store.h"
#include "coco/transport/http_response.h"
namespace coco {
HttpResponse DashboardResponse(const std::string& path, const std::string& web_root,
                               const Context& context, Clock::time_point now, EventStore& store,
                               int64_t reference_utc);
}  // namespace coco
#endif
