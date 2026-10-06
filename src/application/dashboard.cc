#include "coco/application/dashboard.h"

#include <fstream>
#include <map>
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
  if (path == "/v1/timeline" || path.rfind("/v1/timeline?", 0) == 0) {
    try {
      std::map<std::string, std::string> parameters;
      if (path != "/v1/timeline") {
        std::istringstream query(path.substr(13));
        std::string part;
        while (std::getline(query, part, '&')) {
          const auto equal = part.find('=');
          const auto key = part.substr(0, equal);
          if (equal == std::string::npos || (key != "day" && key != "start" && key != "end") ||
              !parameters.emplace(key, part.substr(equal + 1)).second) {
            throw std::invalid_argument("invalid query");
          }
        }
        if (!parameters.count("day") || path.back() == '&') {
          throw std::invalid_argument("missing day");
        }
      }
      const std::string day =
          parameters.empty() ? FormatUtcSeconds(reference).substr(0, 10) : parameters.at("day");
      if (parameters.count("start") != parameters.count("end")) {
        throw std::invalid_argument("incomplete window");
      }
      if (parameters.count("start")) {
        auto seconds = [&](const std::string& key) {
          const auto& value = parameters.at(key);
          if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
            throw std::invalid_argument("invalid seconds");
          }
          try {
            return std::stoll(value);
          } catch (const std::out_of_range&) {
            throw std::invalid_argument("seconds overflow");
          }
        };
        return {200, store.WindowTimeline(context.source_id, day, seconds("start"), seconds("end"),
                                          reference)};
      }
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
  } else if (path == "/state-view.js") {
    file = "state-view.js";
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
