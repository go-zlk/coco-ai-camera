#include <iostream>
#include <stdexcept>

#include "coco/domain/presence_state.h"
#include "coco/storage/event_store.h"
void Require(bool ok) {
  if (!ok) {
    throw std::runtime_error("State/store check failed");
  }
}
int main() {
  coco::PresenceState state("person");
  auto t = coco::Clock::now();
  auto Update = [&](bool seen, bool online, int seconds) {
    return state.Update(seen, .9f, online, t + std::chrono::seconds(seconds), "test",
                        "2026-01-01T00:00:00Z");
  };
  Require(!Update(true, true, 0));
  Require(Update(true, true, 1).has_value());
  Require(!Update(false, true, 2));
  Require(!Update(true, true, 3));
  Require(!Update(false, true, 4));
  Require(Update(false, true, 9).has_value());
  Require(state.state() == "out_of_view");
  Require(Update(false, false, 10).has_value());
  Require(!Update(true, true, 11));
  Require(Update(true, true, 12).has_value());
  Require(!Update(true, true, 13));
  coco::EventStore store(":memory:");
  store.Append({"test", "person", "visible", "time", .9f});
  Require(store.Timeline().find("visible") != std::string::npos);
  Require(coco::JsonString("a\"\n") == "\"a\\\"\\u000a\"");
  std::cout << "domain debounce, offline/recovery, SQLite and JSON checks passed\n";
}
