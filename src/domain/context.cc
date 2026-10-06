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
std::string ContextJson(const Context& c, Clock::time_point now) {
  double age = c.updated == Clock::time_point{}
                   ? -1
                   : std::chrono::duration<double>(now - c.updated).count();
  std::ostringstream s;
  s << "{\"source_id\":" << JsonString(c.source_id) << ",\"mode\":" << JsonString(c.mode)
    << ",\"pet_state\":" << JsonString(c.pet_state) << ",\"pet_confidence\":" << c.pet_confidence
    << ",\"pet_evidence\":" << JsonString(c.pet_evidence)
    << ",\"pet_last_seen_seconds\":" << c.pet_last_seen_seconds
    << ",\"observed_at\":" << JsonString(c.observed_at) << ",\"health\":" << JsonString(c.health)
    << ",\"person\":" << JsonString(c.person) << ",\"cat\":" << JsonString(c.cat)
    << ",\"person_count\":" << c.person_count << ",\"cat_count\":" << c.cat_count
    << ",\"sequence\":" << c.sequence << ",\"dropped_frames\":" << c.dropped_frames
    << ",\"inference_ms\":" << c.inference_ms << ",\"freshness_seconds\":" << age
    << ",\"timeline_window_supported\":true,\"pet_portrait\":{\"status\":"
    << JsonString(c.pet_portrait.status) << ",\"name\":" << JsonString(c.pet_portrait.name)
    << ",\"track_id\":" << c.pet_portrait.track_id << ",\"sequence\":" << c.pet_portrait.sequence
    << ",\"observed_at\":" << JsonString(c.pet_portrait.observed_at)
    << ",\"similarity\":" << c.pet_portrait.similarity
    << ",\"thumbnail_available\":" << (c.pet_portrait.thumbnail_available ? "true" : "false")
    << "},\"tracks\":[";
  bool first = true;
  for (const auto& track : c.tracks) {
    if (!first) {
      s << ',';
    }
    first = false;
    s << "{\"track_id\":" << track.track_id << ",\"class_id\":" << track.class_id
      << ",\"status\":" << JsonString(track.status)
      << ",\"observed\":" << (track.observed ? "true" : "false")
      << ",\"confirmed\":" << (track.confirmed ? "true" : "false")
      << ",\"confidence\":" << track.confidence << ",\"last_seen_age_seconds\":"
      << std::chrono::duration<double>(now - track.last_seen).count()
      << ",\"bbox\":{\"x\":" << track.box.x << ",\"y\":" << track.box.y
      << ",\"width\":" << track.box.width << ",\"height\":" << track.box.height << "}}";
  }
  s << "]}";
  return s.str();
}
}  // namespace coco
