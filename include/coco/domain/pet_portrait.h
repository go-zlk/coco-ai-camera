#ifndef COCO_DOMAIN_PET_PORTRAIT_H_
#define COCO_DOMAIN_PET_PORTRAIT_H_
#include <cstdint>
#include <string>
namespace coco {
struct PetPortrait {
  std::string status = "unavailable", name, observed_at;
  uint64_t track_id = 0, sequence = 0;
  double similarity = 0;
  bool thumbnail_available = false;
};
}  // namespace coco
#endif
