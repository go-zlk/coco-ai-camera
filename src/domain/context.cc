#include "coco/domain/context.h"

#include <ctime>
#include <iomanip>
#include <sstream>
namespace coco {
std::string UtcNow() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  std::ostringstream s;
  s << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return s.str();
}
std::string JsonString(const std::string& value) {
  std::ostringstream s;
  s << '"';
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') {
      s << '\\' << c;
    } else if (c < 32) {
      s << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
    } else {
      s << c;
    }
  }
  s << '"';
  return s.str();
}
std::string ContextJson(const Context& c) {
  double age = c.updated == Clock::time_point{}
                   ? -1
                   : std::chrono::duration<double>(Clock::now() - c.updated).count();
  std::ostringstream s;
  s << "{\"source_id\":" << JsonString(c.source_id)
    << ",\"observed_at\":" << JsonString(c.observed_at) << ",\"health\":" << JsonString(c.health)
    << ",\"person\":" << JsonString(c.person) << ",\"cat\":" << JsonString(c.cat)
    << ",\"person_count\":" << c.person_count << ",\"cat_count\":" << c.cat_count
    << ",\"sequence\":" << c.sequence << ",\"dropped_frames\":" << c.dropped_frames
    << ",\"inference_ms\":" << c.inference_ms << ",\"freshness_seconds\":" << age << '}';
  return s.str();
}
}  // namespace coco
