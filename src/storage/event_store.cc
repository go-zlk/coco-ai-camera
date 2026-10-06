#include "coco/storage/event_store.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "coco/domain/utc_time.h"
namespace coco {
namespace {
struct Statement {
  sqlite3_stmt* value = nullptr;
  Statement(sqlite3* db, const char* sql) {
    if (sqlite3_prepare_v2(db, sql, -1, &value, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db));
    }
  }
  ~Statement() {
    sqlite3_finalize(value);
  }
};
}  // namespace
EventStore::EventStore(const std::string& path) {
  auto parent = std::filesystem::path(path).parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    if (db_) {
      sqlite3_close(db_);
    }
    db_ = nullptr;
    throw std::runtime_error("Cannot open event database");
  }
  sqlite3_busy_timeout(db_, 3000);
  // Separate table: never reinterpret or migrate the Python identity schema.
  const char* sql =
      "PRAGMA journal_mode=WAL; CREATE TABLE IF NOT EXISTS context_events (id INTEGER PRIMARY KEY, "
      "source_id TEXT NOT NULL, category TEXT NOT NULL, state TEXT NOT NULL, observed_at TEXT NOT "
      "NULL, confidence REAL NOT NULL); CREATE INDEX IF NOT EXISTS context_event_time ON "
      "context_events(observed_at);"
      "CREATE TABLE IF NOT EXISTS pet_intervals (id INTEGER PRIMARY KEY,source_id TEXT NOT NULL,"
      "state TEXT NOT NULL,confidence REAL NOT NULL,start_ts INTEGER NOT NULL,end_ts INTEGER NOT "
      "NULL,"
      "last_seen_ts INTEGER NOT NULL,is_open INTEGER NOT NULL);"
      "CREATE INDEX IF NOT EXISTS pet_interval_source_time ON pet_intervals(source_id,start_ts);";
  if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    std::string error = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error(error);
  }
}
EventStore::~EventStore() {
  if (db_) {
    sqlite3_close(db_);
  }
}
void EventStore::Append(const Event& e) {
  std::lock_guard<std::mutex> lock(mutex_);
  Statement s(db_,
              "INSERT INTO context_events(source_id,category,state,observed_at,confidence) VALUES "
              "(?,?,?,?,?)");
  const std::string* strings[] = {&e.source_id, &e.category, &e.state, &e.observed_at};
  for (int i = 0; i < 4; ++i) {
    sqlite3_bind_text(s.value, i + 1, strings[i]->c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_double(s.value, 5, e.confidence);
  if (sqlite3_step(s.value) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
}
std::string EventStore::Timeline() {
  std::lock_guard<std::mutex> lock(mutex_);
  Statement s(db_,
              "SELECT source_id,category,state,observed_at,confidence FROM context_events ORDER BY "
              "id DESC LIMIT 100");
  std::ostringstream out;
  out << "{\"events\":[";
  bool first = true;
  int rc;
  while ((rc = sqlite3_step(s.value)) == SQLITE_ROW) {
    if (!first) {
      out << ',';
    }
    first = false;
    out << '{';
    const char* keys[] = {"source_id", "category", "state", "observed_at"};
    for (int i = 0; i < 4; ++i) {
      if (i) {
        out << ',';
      }
      out << JsonString(keys[i]) << ':'
          << JsonString(reinterpret_cast<const char*>(sqlite3_column_text(s.value, i)));
    }
    out << ",\"confidence\":" << sqlite3_column_double(s.value, 4) << '}';
  }
  if (rc != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
  out << "]}";
  return out.str();
}
void EventStore::RecordContext(const Context& context, int64_t at) {
  if (at < 0 || context.source_id.empty()) {
    throw std::invalid_argument("Invalid interval timestamp/source");
  }
  static const std::vector<std::string> states{"active", "resting", "unknown", "offline",
                                               "out_of_view"};
  if (std::find(states.begin(), states.end(), context.pet_state) == states.end()) {
    throw std::invalid_argument("Invalid pet state");
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
  try {
    int64_t id = 0, last = 0;
    std::string previous;
    Statement query(db_,
                    "SELECT id,state,last_seen_ts FROM pet_intervals WHERE source_id=? ORDER BY id "
                    "DESC LIMIT 1");
    sqlite3_bind_text(query.value, 1, context.source_id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(query.value);
    if (rc == SQLITE_ROW) {
      id = sqlite3_column_int64(query.value, 0);
      previous = reinterpret_cast<const char*>(sqlite3_column_text(query.value, 1));
      last = sqlite3_column_int64(query.value, 2);
    } else if (rc != SQLITE_DONE) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
    auto execute = [&](Statement& s) {
      if (sqlite3_step(s.value) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db_));
      }
    };
    auto insert = [&](const std::string& state, int64_t begin, int64_t end, bool open,
                      float confidence) {
      Statement s(db_,
                  "INSERT INTO "
                  "pet_intervals(source_id,state,confidence,start_ts,end_ts,last_seen_ts,is_open) "
                  "VALUES(?,?,?,?,?,?,?)");
      sqlite3_bind_text(s.value, 1, context.source_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s.value, 2, state.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_double(s.value, 3, confidence);
      sqlite3_bind_int64(s.value, 4, begin);
      sqlite3_bind_int64(s.value, 5, end);
      sqlite3_bind_int64(s.value, 6, end);
      sqlite3_bind_int(s.value, 7, open);
      execute(s);
    };
    if (!id) {
      insert(context.pet_state, at, at, true, context.pet_confidence);
    } else if (at > last) {
      bool same = previous == context.pet_state && at - last <= 5;
      int64_t end = std::min(at, last + 5);
      Statement close(db_, "UPDATE pet_intervals SET end_ts=?,last_seen_ts=?,is_open=? WHERE id=?");
      sqlite3_bind_int64(close.value, 1, end);
      sqlite3_bind_int64(close.value, 2, same ? at : last);
      sqlite3_bind_int(close.value, 3, same);
      sqlite3_bind_int64(close.value, 4, id);
      execute(close);
      if (!same) {
        if (at > end) {
          insert("unknown", end, at, false, 0);
        }
        insert(context.pet_state, at, at, true, context.pet_confidence);
      }
    }
    if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
  }
}
std::string EventStore::DayTimeline(const std::string& source, const std::string& day,
                                    int64_t now) {
  int64_t begin = ParseUtcSeconds(day + "T00:00:00Z"), end = std::min(begin + 86400, now);
  std::lock_guard<std::mutex> lock(mutex_);
  Statement query(db_,
                  "SELECT state,start_ts,end_ts,last_seen_ts,is_open FROM pet_intervals WHERE "
                  "source_id=? AND start_ts<? AND last_seen_ts+5>? ORDER BY start_ts,id");
  sqlite3_bind_text(query.value, 1, source.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(query.value, 2, end);
  sqlite3_bind_int64(query.value, 3, begin);
  std::map<std::string, int64_t> totals{
      {"active", 0}, {"resting", 0}, {"out_of_view", 0}, {"offline", 0}, {"unknown", 0}};
  std::ostringstream out;
  out << "{\"day\":" << JsonString(day) << ",\"timezone\":\"UTC\",\"intervals\":[";
  bool first = true;
  int rc;
  int64_t cursor = begin;
  auto emit = [&](const std::string& state, int64_t from, int64_t to) {
    if (to <= from) {
      return;
    }
    if (!first) {
      out << ',';
    }
    first = false;
    out << "{\"state\":" << JsonString(state) << ",\"start\":" << JsonString(FormatUtcSeconds(from))
        << ",\"end\":" << JsonString(FormatUtcSeconds(to))
        << ",\"duration_seconds\":" << (to - from) << '}';
    totals[state] += to - from;
  };
  while ((rc = sqlite3_step(query.value)) == SQLITE_ROW) {
    std::string state = reinterpret_cast<const char*>(sqlite3_column_text(query.value, 0));
    int64_t from = std::max<int64_t>(begin, sqlite3_column_int64(query.value, 1));
    int64_t to = std::min<int64_t>(end, sqlite3_column_int64(query.value, 4)
                                            ? sqlite3_column_int64(query.value, 3) + 5
                                            : sqlite3_column_int64(query.value, 2));
    if (from > cursor) {
      emit("unknown", cursor, std::min(from, end));
    }
    from = std::max(from, cursor);
    emit(state, from, to);
    cursor = std::max(cursor, to);
  }
  if (rc != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
  emit("unknown", cursor, end);
  out << "],\"summary_seconds\":{";
  first = true;
  for (const auto& total : totals) {
    if (!first) {
      out << ',';
    }
    first = false;
    out << JsonString(total.first) << ':' << total.second;
  }
  out << "}}";
  return out.str();
}
}  // namespace coco
