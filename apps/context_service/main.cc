#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "coco/application/context_service.h"
#include "coco/application/service_config.h"

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void HandleInterrupt(int) {
  stop_requested = 1;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    auto parsed = coco::ParseServiceConfig(std::vector<std::string>(argv + 1, argv + argc));
    if (parsed.help) {
      std::cout << coco::ServiceUsage();
      return 0;
    }
    std::signal(SIGINT, HandleInterrupt);
    std::signal(SIGTERM, HandleInterrupt);
    return coco::RunContextService(parsed.config, [] { return stop_requested != 0; });
  } catch (const std::exception& error) {
    std::cerr << "[fatal] " << error.what() << '\n';
    return 1;
  }
}
