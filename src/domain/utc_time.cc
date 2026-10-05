#include "coco/domain/utc_time.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace coco {
std::string FormatUtcSeconds(int64_t seconds) {
  std::time_t time = seconds;
  std::tm tm{};
  if (!gmtime_r(&time, &tm)) {
    throw std::invalid_argument("Invalid UTC time");
  }
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}
int64_t ParseUtcSeconds(const std::string& timestamp) {
  if (timestamp.size() != 20) {
    throw std::invalid_argument("Expected UTC YYYY-MM-DDTHH:MM:SSZ");
  }
  std::tm tm{};
  std::istringstream input(timestamp);
  input >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  if (input.fail()) {
    throw std::invalid_argument("Invalid UTC timestamp");
  }
  auto seconds = timegm(&tm);
  if (seconds < 0 || FormatUtcSeconds(seconds) != timestamp) {
    throw std::invalid_argument("Invalid UTC calendar date");
  }
  return seconds;
}
}  // namespace coco
