#ifndef COCO_DOMAIN_UTC_TIME_H_
#define COCO_DOMAIN_UTC_TIME_H_
#include <cstdint>
#include <string>
namespace coco {
int64_t ParseUtcSeconds(const std::string& timestamp);
std::string FormatUtcSeconds(int64_t seconds);
}  // namespace coco
#endif  // COCO_DOMAIN_UTC_TIME_H_
