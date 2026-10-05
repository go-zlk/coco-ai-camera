#ifndef COCO_TRANSPORT_CONTEXT_API_H_
#define COCO_TRANSPORT_CONTEXT_API_H_
#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include "coco/transport/http_response.h"
namespace coco {
// Small loopback-only read API; not an internet-facing HTTP server.
class ContextApi {
 public:
  ContextApi(int port, std::function<std::string(const std::string&)> handler);
  ContextApi(int port, std::function<HttpResponse(const std::string&)> handler);
  ~ContextApi();
  int port() const {
    return port_;
  }
  void Start();
  void Stop();

 private:
  void Serve();
  int port_ = 0;
  int socket_ = -1;
  std::atomic<bool> running_{false};
  std::thread thread_;
  std::function<HttpResponse(const std::string&)> handler_;
};
}  // namespace coco

#endif  // COCO_TRANSPORT_CONTEXT_API_H_
