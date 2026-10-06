#include "coco/application/service_config.h"

#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace coco {
namespace {
int Integer(const std::string& value) {
  size_t end = 0;
  int number = std::stoi(value, &end);
  if (end != value.size()) {
    throw std::invalid_argument("Expected a complete integer value");
  }
  return number;
}
double Number(const std::string& value) {
  size_t end = 0;
  double number = std::stod(value, &end);
  if (end != value.size() || !std::isfinite(number)) {
    throw std::invalid_argument("Expected a finite numeric value");
  }
  return number;
}
}  // namespace

void ValidateServiceConfig(const ServiceConfig& config) {
  if (config.source.empty() || config.model.empty() || config.source_id.empty() ||
      config.database.empty() || (config.backend != "onnx" && config.backend != "tensorrt") ||
      config.port < 1 || config.port > 65535 || config.image_size < 32 || config.interval_ms < 1 ||
      !std::isfinite(config.duration_seconds) || config.duration_seconds < 0 ||
      !std::isfinite(config.confidence) || config.confidence <= 0 || config.confidence >= 1) {
    throw std::invalid_argument("Invalid service configuration");
  }
}

ParsedServiceConfig ParseServiceConfig(const std::vector<std::string>& arguments) {
  if (arguments.size() == 1 && arguments.front() == "--help") {
    return {ServiceConfig{}, true};
  }
  std::map<std::string, std::string> values{
      {"--web-root", "web"},       {"--source", ""},          {"--model", ""},
      {"--source-id", "camera_1"}, {"--backend", "tensorrt"}, {"--db", "data/context.db"},
      {"--port", "8090"},          {"--imgsz", "640"},        {"--conf", "0.25"},
      {"--interval-ms", "1000"},   {"--seconds", "0"},        {"--pet-gallery", ""}};
  std::set<std::string> seen;
  for (size_t index = 0; index < arguments.size(); index += 2) {
    const auto& key = arguments[index];
    if (!values.count(key) || !seen.insert(key).second || index + 1 >= arguments.size() ||
        arguments[index + 1].rfind("--", 0) == 0) {
      throw std::invalid_argument("Unknown, duplicate argument or missing value: " + key);
    }
    values[key] = arguments[index + 1];
  }
  ServiceConfig config;
  config.web_root = values.at("--web-root");
  config.pet_gallery = values.at("--pet-gallery");
  config.source = values.at("--source");
  config.model = values.at("--model");
  config.source_id = values.at("--source-id");
  config.backend = values.at("--backend");
  config.database = values.at("--db");
  config.port = Integer(values.at("--port"));
  config.image_size = Integer(values.at("--imgsz"));
  config.interval_ms = Integer(values.at("--interval-ms"));
  config.confidence = static_cast<float>(Number(values.at("--conf")));
  config.duration_seconds = Number(values.at("--seconds"));
  ValidateServiceConfig(config);
  return {config, false};
}

std::string ServiceUsage() {
  return "coco-context --source <RTSP|csi|USB index> --model <raw.engine|model.onnx> "
         "[--backend tensorrt|onnx] [--db data/context.db] [--source-id camera_1] "
         "[--web-root web] [--port 8090] [--seconds 0] [--interval-ms 1000] [--imgsz 640] [--conf "
         "0.25] [--pet-gallery data/gallery.json]\n";
}
}  // namespace coco
