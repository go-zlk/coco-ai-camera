#include "coco/store.hpp"
#include <filesystem>
#include <sstream>
#include <stdexcept>
namespace coco {
namespace {
struct Statement {
  sqlite3_stmt* value=nullptr;
  Statement(sqlite3* db,const char* sql) { if (sqlite3_prepare_v2(db,sql,-1,&value,nullptr)!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db)); }
  ~Statement() { sqlite3_finalize(value); }
};
}
EventStore::EventStore(const std::string& path) {
  auto parent=std::filesystem::path(path).parent_path(); if (!parent.empty()) std::filesystem::create_directories(parent);
  if (sqlite3_open(path.c_str(),&db_)!=SQLITE_OK) { if(db_) sqlite3_close(db_); db_=nullptr; throw std::runtime_error("Cannot open event database"); }
  sqlite3_busy_timeout(db_,3000);
  // Separate table: never reinterpret or migrate the Python identity schema.
  const char* sql="PRAGMA journal_mode=WAL; CREATE TABLE IF NOT EXISTS context_events (id INTEGER PRIMARY KEY, source_id TEXT NOT NULL, category TEXT NOT NULL, state TEXT NOT NULL, observed_at TEXT NOT NULL, confidence REAL NOT NULL); CREATE INDEX IF NOT EXISTS context_event_time ON context_events(observed_at);";
  if (sqlite3_exec(db_,sql,nullptr,nullptr,nullptr)!=SQLITE_OK) { std::string error=sqlite3_errmsg(db_); sqlite3_close(db_); db_=nullptr; throw std::runtime_error(error); }
}
EventStore::~EventStore() { if(db_) sqlite3_close(db_); }
void EventStore::append(const Event& e) {
  std::lock_guard<std::mutex> lock(mutex_);
  Statement s(db_,"INSERT INTO context_events(source_id,category,state,observed_at,confidence) VALUES (?,?,?,?,?)");
  const std::string* strings[]={&e.source_id,&e.category,&e.state,&e.observed_at};
  for(int i=0;i<4;++i) sqlite3_bind_text(s.value,i+1,strings[i]->c_str(),-1,SQLITE_TRANSIENT);
  sqlite3_bind_double(s.value,5,e.confidence);
  if(sqlite3_step(s.value)!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
}
std::string EventStore::timeline() {
  std::lock_guard<std::mutex> lock(mutex_);
  Statement s(db_,"SELECT source_id,category,state,observed_at,confidence FROM context_events ORDER BY id DESC LIMIT 100");
  std::ostringstream out; out << "{\"events\":["; bool first=true; int rc;
  while((rc=sqlite3_step(s.value))==SQLITE_ROW) {
    if(!first) out << ',';
    first=false;
    out << '{';
    const char* keys[]={"source_id","category","state","observed_at"};
    for(int i=0;i<4;++i) { if(i) out<<','; out<<json_string(keys[i])<<':'<<json_string(reinterpret_cast<const char*>(sqlite3_column_text(s.value,i))); }
    out << ",\"confidence\":" << sqlite3_column_double(s.value,4) << '}';
  }
  if(rc!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
  out << "]}"; return out.str();
}
}
