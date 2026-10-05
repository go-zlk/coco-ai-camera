#include "coco/application/replay_service.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "coco/storage/event_store.h"

namespace {
void Require(bool ok, const char* message) {
  if (!ok) {
    throw std::runtime_error(message);
  }
}
struct Workspace {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("coco-replay-test-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Workspace() {
    std::filesystem::create_directories(path);
  }
  ~Workspace() {
    std::filesystem::remove_all(path);
  }
};
}  // namespace
int main() {
  Workspace workspace;
  coco::ReplayConfig config;
  config.input = COCO_REPLAY_FIXTURE;
  config.database = (workspace.path / "events.db").string();
  std::ostringstream output;
  Require(coco::RunReplayService(config, output) == 13, "fixture row count");
  Require(output.str().find("\"mode\":\"replay\"") != std::string::npos, "simulation label");
  Require(output.str().find("\"health\":\"offline\"") != std::string::npos, "offline transition");
  coco::EventStore store(config.database);
  sqlite3* database = nullptr;
  Require(sqlite3_open(config.database.c_str(), &database) == SQLITE_OK, "open replay database");
  sqlite3_stmt* statement = nullptr;
  int rc = sqlite3_prepare_v2(database, "SELECT category,state FROM context_events ORDER BY id", -1,
                              &statement, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_close(database);
    throw std::runtime_error("Prepare failed");
  }
  std::string transitions;
  while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
    transitions += reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
    transitions += ":";
    transitions += reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    transitions += ";";
  }
  sqlite3_finalize(statement);
  sqlite3_close(database);
  Require(rc == SQLITE_DONE, "read transitions");
  Require(transitions ==
              "person:visible;cat:visible;cat:out_of_view;person:offline;cat:offline;person:"
              "visible;cat:visible;",
          "persisted event sequence");
  auto malformed = workspace.path / "malformed.csv";
  std::ofstream file(malformed);
  file << "elapsed_ms,kind,person_count,cat_count,person_confidence,cat_confidence,observed_at\n"
          "0,tick,1,0,0,0,time\n";
  file.close();
  config.input = malformed.string();
  config.database = ":memory:";
  bool rejected = false;
  try {
    coco::RunReplayService(config, output);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  Require(rejected, "invalid tick must not invent a detection");
  config.input = COCO_TRACKING_FIXTURE;
  config.database = ":memory:";
  std::ostringstream tracking_output;
  Require(coco::RunReplayService(config, tracking_output) == 12, "tracking fixture row count");
  Require(tracking_output.str().find("\"status\":\"lost\"") != std::string::npos,
          "tracking replay retains lost history");
  Require(tracking_output.str().find("\"tracks\":[]") != std::string::npos,
          "offline tracking snapshot is cleared");
  Require(tracking_output.str().find("\"track_id\":3") != std::string::npos,
          "stream recovery receives a new track ID");
}
