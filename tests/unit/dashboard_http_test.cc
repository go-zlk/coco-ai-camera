#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

#include "coco/application/dashboard.h"
#include "coco/domain/utc_time.h"
#include "coco/transport/context_api.h"
namespace {
void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}
}  // namespace
int main() {
  coco::EventStore store(":memory:");
  coco::Context context;
  context.source_id = "test";
  auto utc = coco::ParseUtcSeconds("2026-01-01T00:00:10Z");
  store.RecordContext(context, utc - 5);
  coco::ContextApi api(0, [&](const std::string& path) -> coco::HttpResponse {
    return coco::DashboardResponse(path, COCO_WEB_FIXTURE, context, coco::Clock::now(), store, utc);
  });
  api.Start();
  auto request = [&](const std::string& path) {
    int client = socket(AF_INET, SOCK_STREAM, 0);
    Require(client >= 0, "socket");
    timeval timeout{2, 0};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(api.port());
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
      close(client);
      throw std::runtime_error("connect failed");
    }
    std::string query = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(client, query.data(), query.size(), 0);
    std::string response;
    char bytes[4096];
    ssize_t size;
    while ((size = recv(client, bytes, sizeof(bytes), 0)) > 0) {
      response.append(bytes, size);
    }
    close(client);
    return response;
  };
  auto page = request("/");
  Require(page.find("text/html") != std::string::npos && page.find("Coco") != std::string::npos,
          "HTML delivery");
  Require(request("/state-view.js").find("companionView") != std::string::npos,
          "presentation script delivery");
  Require(request("/v1/context/current").find("\"pet_state\":\"unknown\"") != std::string::npos,
          "snapshot route");
  Require(request("/v1/timeline?day=2026-01-01").find("summary_seconds") != std::string::npos,
          "timeline route");
  Require(request("/v1/timeline?day=wrong").find("400 Response") != std::string::npos,
          "bad day route");
  Require(request("/../README.md").find("404 Response") != std::string::npos, "asset whitelist");
  api.Stop();
}
