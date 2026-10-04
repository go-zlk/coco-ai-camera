#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <thread>
namespace coco {
// Small loopback-only read API; not an internet-facing HTTP server.
class ContextApi {
public:
  ContextApi(int port,std::function<std::string(const std::string&)> handler);
  ~ContextApi();
  void start();
  void stop();
private:
  void serve();
  int socket_=-1;
  std::atomic<bool> running_{false};
  std::thread thread_;
  std::function<std::string(const std::string&)> handler_;
};
}
