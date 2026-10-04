#include "coco/transport/context_api.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>
#include <utility>
namespace coco {
ContextApi::ContextApi(int port, std::function<std::string(const std::string&)> handler)
    : handler_(std::move(handler)) {
  socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ < 0) {
    throw std::runtime_error("Cannot create API socket");
  }
  int yes = 1;
  setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
      listen(socket_, 8) < 0) {
    close(socket_);
    socket_ = -1;
    throw std::runtime_error("API bind failed; check port availability");
  }
}
ContextApi::~ContextApi() {
  Stop();
  if (socket_ >= 0) {
    close(socket_);
  }
}
void ContextApi::Start() {
  running_ = true;
  thread_ = std::thread(&ContextApi::Serve, this);
}
void ContextApi::Stop() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}
void ContextApi::Serve() {
  while (running_) {
    pollfd fd{socket_, POLLIN, 0};
    if (poll(&fd, 1, 200) <= 0) {
      continue;
    }
    int client = accept(socket_, nullptr, nullptr);
    if (client < 0) {
      continue;
    }
    timeval timeout{2, 0};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    std::string request;
    char bytes[1024];
    while (request.size() < 8192 && request.find("\r\n\r\n") == std::string::npos) {
      auto count = recv(client, bytes, sizeof(bytes), 0);
      if (count <= 0) {
        break;
      }
      request.append(bytes, count);
    }
    std::string body = "{\"error\":\"bad_request\"}";
    int status = 400;
    auto end = request.find(' ', 4);
    if (request.rfind("GET ", 0) == 0 && end != std::string::npos &&
        request.find("\r\n\r\n") != std::string::npos) {
      std::string path = request.substr(4, end - 4);
      try {
        body = handler_(path);
        status = body.empty() ? 404 : 200;
        if (body.empty()) {
          body = "{\"error\":\"not_found\"}";
        }
      } catch (...) {
        status = 500;
        body = "{\"error\":\"internal_error\"}";
      }
    }
    std::string response = "HTTP/1.1 " + std::to_string(status) +
                           " Response\r\nContent-Type: application/json\r\nCache-Control: "
                           "no-store\r\nConnection: close\r\nContent-Length: " +
                           std::to_string(body.size()) + "\r\n\r\n" + body;
    size_t sent = 0;
    while (sent < response.size()) {
      auto n = send(client, response.data() + sent, response.size() - sent, MSG_NOSIGNAL);
      if (n <= 0) {
        break;
      }
      sent += n;
    }
    close(client);
  }
}
}  // namespace coco
