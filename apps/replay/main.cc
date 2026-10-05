#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

#include "coco/application/replay_service.h"
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--help") {
      std::cout << "coco-replay --input observations.csv [--db data/replay_context.db] "
                   "[--source-id replay_camera]\n";
      return 0;
    }
    coco::ReplayConfig config;
    std::set<std::string> seen;
    for (int index = 1; index < argc; index += 2) {
      std::string key = argv[index];
      if (!seen.insert(key).second || index + 1 >= argc ||
          std::string(argv[index + 1]).rfind("--", 0) == 0) {
        throw std::invalid_argument("Duplicate argument or missing value");
      }
      if (key == "--input") {
        config.input = argv[index + 1];
      } else if (key == "--db") {
        config.database = argv[index + 1];
      } else if (key == "--source-id") {
        config.source_id = argv[index + 1];
      } else {
        throw std::invalid_argument("Unknown argument: " + key);
      }
    }
    auto rows = coco::RunReplayService(config, std::cout);
    std::cerr << "[replay] simulated observation rows=" << rows << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "[replay error] " << error.what() << '\n';
    return 1;
  }
}
