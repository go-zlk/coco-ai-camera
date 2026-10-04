#ifndef COCO_STORAGE_EVENT_STORE_H_
#define COCO_STORAGE_EVENT_STORE_H_
#include <sqlite3.h>

#include <mutex>

#include "coco/domain/context.h"
namespace coco {
class EventStore {
 public:
  explicit EventStore(const std::string& path);
  ~EventStore();
  void Append(const Event& event);
  std::string Timeline();
  EventStore(const EventStore&) = delete;
  EventStore& operator=(const EventStore&) = delete;

 private:
  sqlite3* db_ = nullptr;
  std::mutex mutex_;
};
}  // namespace coco

#endif  // COCO_STORAGE_EVENT_STORE_H_
