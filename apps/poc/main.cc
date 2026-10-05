#include <csignal>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

#include "coco/application/poc_service.h"
namespace {
volatile std::sig_atomic_t stopping = 0;
void Stop(int) {
  stopping = 1;
}
}  // namespace
int main(int argc, char** argv) {
  try {
    coco::PocConfig config;
    config.replay.input = "tests/fixtures/pet_poc.csv";
    config.replay.database = "data/poc_replay.db";
    std::set<std::string> seen;
    for (int i = 1; i < argc; i += 2) {
      std::string key = argv[i];
      if (key == "--help") {
        std::cout << "coco-poc [--input file.csv] [--db replay.db] [--port 8091] [--speed 1] "
                     "[--web-root web]\n";
        return 0;
      }
      if (i + 1 >= argc || !seen.insert(key).second) {
        throw std::invalid_argument("Missing or duplicate argument");
      }
      std::string value = argv[i + 1];
      size_t end = 0;
      if (key == "--input") {
        config.replay.input = value;
      } else if (key == "--db") {
        config.replay.database = value;
      } else if (key == "--web-root") {
        config.web_root = value;
      } else if (key == "--port") {
        config.port = std::stoi(value, &end);
        if (end != value.size()) {
          throw std::invalid_argument("Invalid port");
        }
      } else if (key == "--speed") {
        config.replay.speed = std::stod(value, &end);
        if (end != value.size()) {
          throw std::invalid_argument("Invalid speed");
        }
      } else {
        throw std::invalid_argument("Unknown argument");
      }
    }
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    return coco::RunPocService(config, [] { return stopping != 0; });
  } catch (const std::exception& e) {
    std::cerr << "[fatal] " << e.what() << '\n';
    return 1;
  }
}
