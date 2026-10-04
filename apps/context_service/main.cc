#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "coco/application/context_service.h"
int main(int argc, char** argv) {
  try {
    std::map<std::string, std::string> args{{"--source-id", "camera_1"}, {"--backend", "tensorrt"},
                                            {"--db", "data/context.db"}, {"--port", "8090"},
                                            {"--imgsz", "640"},          {"--conf", "0.25"},
                                            {"--interval-ms", "1000"},   {"--seconds", "0"}};
    for (int i = 1; i < argc; ++i) {
      std::string key = argv[i];
      if (key == "--help") {
        std::cout
            << "coco-context --source <RTSP|csi|USB index> --model <raw.engine|model.onnx> "
               "[--backend tensorrt|onnx] [--db data/context.db] [--source-id camera_1] [--port "
               "8090] [--seconds 0] [--interval-ms 1000] [--imgsz 640] [--conf 0.25]\n";
        return 0;
      }
      if ((!args.count(key) && key != "--source" && key != "--model") || i + 1 >= argc) {
        throw std::runtime_error("Unknown argument or missing value");
      }
      args[key] = argv[++i];
    }
    if (!args.count("--source") || !args.count("--model")) {
      throw std::runtime_error("--source and --model are required");
    }
    int port = std::stoi(args["--port"]), size = std::stoi(args["--imgsz"]),
        interval = std::stoi(args["--interval-ms"]);
    double seconds = std::stod(args["--seconds"]);
    float conf = std::stof(args["--conf"]);
    coco::ServiceConfig config;
    config.source = args["--source"];
    config.model = args["--model"];
    config.source_id = args["--source-id"];
    config.backend = args["--backend"];
    config.database = args["--db"];
    config.port = port;
    config.image_size = size;
    config.interval_ms = interval;
    config.duration_seconds = seconds;
    config.confidence = conf;
    return coco::RunContextService(config);
  } catch (const std::exception& error) {
    std::cerr << "[fatal] " << error.what() << '\n';
    return 1;
  }
}
