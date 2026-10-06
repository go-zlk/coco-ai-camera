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
  // Persist pet-state intervals; a gap beyond five seconds is explicitly unknown.
  void RecordContext(const Context& context, int64_t at);
  std::string DayTimeline(const std::string& source, const std::string& utc_day, int64_t now);
  // Explicit UTC bounds allow the client to request its local calendar day.
  std::string WindowTimeline(const std::string& source, const std::string& day, int64_t begin,
                             int64_t end, int64_t now);
  EventStore(const EventStore&) = delete;
  EventStore& operator=(const EventStore&) = delete;

 private:
  sqlite3* db_ = nullptr;
  std::mutex mutex_;
};
}  // namespace coco

#endif  // COCO_STORAGE_EVENT_STORE_H_
