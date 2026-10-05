#include "coco/application/dashboard.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "coco/domain/utc_time.h"
namespace coco {
HttpResponse DashboardResponse(const std::string& path, const std::string& root,
                               const Context& context, Clock::time_point now, EventStore& store,
                               int64_t reference) {
  if (path == "/v1/context/current" || path == "/health") {
    return {200, ContextJson(context, now)};
  }
  if (path == "/v1/events") {
    return {200, store.Timeline()};
  }
  if (path == "/v1/timeline" || path.rfind("/v1/timeline?day=", 0) == 0) {
    try {
      std::string day =
          path == "/v1/timeline" ? FormatUtcSeconds(reference).substr(0, 10) : path.substr(17);
      return {200, store.DayTimeline(context.source_id, day, reference)};
    } catch (const std::invalid_argument&) {
      return {400, "{\"error\":\"invalid_utc_day\"}"};
    }
  }
  std::string file, type;
  if (path == "/" || path == "/index.html") {
    file = "index.html";
    type = "text/html; charset=utf-8";
  } else if (path == "/app.js") {
    file = "app.js";
    type = "text/javascript; charset=utf-8";
  } else if (path == "/styles.css") {
    file = "styles.css";
    type = "text/css; charset=utf-8";
  } else {
    return {404, "{\"error\":\"not_found\"}"};
  }
  std::ifstream input(root + "/" + file);
  if (!input) {
    return {503, "{\"error\":\"web_assets_missing\"}"};
  }
  std::ostringstream body;
  body << input.rdbuf();
  return {200, body.str(), type};
}
}  // namespace coco
