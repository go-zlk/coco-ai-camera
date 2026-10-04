#pragma once
#include "coco/types.hpp"
#include <mutex>
#include <sqlite3.h>
namespace coco {
class EventStore {
public:
  explicit EventStore(const std::string& path);
  ~EventStore();
  void append(const Event& event);
  std::string timeline();
  EventStore(const EventStore&)=delete;
  EventStore& operator=(const EventStore&)=delete;
private:
  sqlite3* db_=nullptr;
  std::mutex mutex_;
};
}
