#ifndef COCO_TRANSPORT_HTTP_RESPONSE_H_
#define COCO_TRANSPORT_HTTP_RESPONSE_H_
#include <string>
namespace coco {
struct HttpResponse {
  int status = 200;
  std::string body;
  std::string content_type = "application/json";
};
}  // namespace coco
#endif
