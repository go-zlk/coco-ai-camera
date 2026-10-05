# C++ 本地感知框架

C++17 常驻程序，将摄像头观察转成当前状态和可查询事件。Python 原型位于 `prototypes/python/`，保留用于采集、模型训练/导出和结果对比；C++ 运行时无需启动 Python 推理服务。

## 模块

| 模块 | 实现 | 职责 |
|---|---|---|
| CameraSource | src/capture/camera_source.cc | RTSP、CSI、USB、文件接入；独立线程、单帧缓冲、重连 |
| Detector | src/perception/detector.cc、src/perception/tensorrt_detector.cc | 模型接口；ONNX/OpenCV CPU、TensorRT 10 GPU 后端 |
| PresenceState | src/domain/presence_state.cc | 每个类别的可见/不可见/离线状态，持续时间确认 |
| EventStore | src/storage/event_store.cc | SQLite WAL，只记录状态变化 |
| ContextApi | src/transport/context_api.cc | 本机只读当前状态、最近事件查询 |
| 应用入口 | src/application/context_service.cc、apps/context_service/main.cc | 生命周期、信号退出、抽帧调度、模块编排 |

当前是一条视频源、人物/猫两个类别的聚合状态，另提供短期 tracks 快照。类别计数是该采样帧的检测框数，不是独立身份数。可见确认约 1 秒，不可见确认约 5 秒；采集 5 秒无新帧后变为离线。只在成功推理时判断不可见，断流单独处理。时间窗口使用单调时钟，记录使用 UTC。时间为本机接收时间，不是摄像头曝光时间。API 提供 freshness_seconds，消费端应拒绝过期状态。

已包含短期几何主体跟踪；暂未包含真实身份、睡眠判断、姿态、场景位置融合、LLM 工作流、前端、访问认证、多摄像头调度及自动数据清理。人物离开视野不等于离家。后续能力通过 Detector 和状态层扩展，不能直接沿用 Python 的身份结果当作 C++ 已实现功能。

## 编译

依赖：CMake、C++17 编译器、OpenCV 4（core/imgproc/videoio/dnn）、SQLite3、线程库。Jetson GPU 后端额外依赖 CUDA Toolkit 和 TensorRT 10 开发库。

```bash
cmake -S . -B build -DCOCO_TENSORRT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

CPU ONNX 后端可用 `-DCOCO_TENSORRT=OFF` 构建。网络读取超时依赖 OpenCV FFmpeg 实现；CSI/USB 驱动读取没有同等超时保证。当前本地 API 使用 POSIX socket，兼容 Linux/macOS 的 SIGPIPE 处理，主验证平台仍为 Jetson Linux。

## 模型准备

当前两个后端要求固定尺寸、batch=1、COCO 80 类的 YOLOv8 检测模型，输出 `[1,84,N]`，不包含嵌入式 NMS。使用 Python 导出阶段准备 ONNX：

```bash
python -c "from ultralytics import YOLO; YOLO('yolov8n.pt').export(format='onnx', imgsz=640, opset=12, simplify=False, dynamic=False)"
```

在目标 Jetson 构建原生 TensorRT plan：

```bash
/usr/src/tensorrt/bin/trtexec --onnx=yolov8n.onnx --saveEngine=yolov8n_raw.engine --fp16 --builderOptimizationLevel=0 --skipInference
```

优化级别 0 用于尽快生成首个功能验证引擎；性能评估时再比较较高优化级别。TensorRT 后端使用 FP32 IO、内部允许 FP16；不接受动态 shape、INT8 IO、分割模型、已内嵌 NMS 的输出或 Ultralytics 带元数据头的 engine 文件。engine 需在目标硬件/软件版本上生成。预处理当前通过 CPU OpenCV letterbox、RGB NCHW、归一化，再上传 GPU；这是功能基线，尚未实现 NVMM/CUDA 零拷贝。

## 运行

先在同一主机配置和启动 go2rtc，保存摄像头视频源。通用示例：

```bash
OPENCV_FFMPEG_CAPTURE_OPTIONS='rtsp_transport;tcp' \
./build/coco-context \
  --source rtsp://127.0.0.1:8554/xiaomi_living_room \
  --source-id camera_1 \
  --backend tensorrt --model yolov8n_raw.engine \
  --db data/context.db --port 8090
```

`--seconds 30` 限定运行时间，默认为持续运行；`--interval-ms 1000` 控制推理间隔。单帧缓冲会丢弃积压帧，避免无限队列；丢弃帧数不是摄像头网络丢包统计。SIGINT/SIGTERM 请求退出，RTSP 退出还可能等待当前读流超时。模型和数据库初始化失败时启动失败，推理错误会退出，不能用伪检测结果替代故障。

```bash
curl http://127.0.0.1:8090/health
curl http://127.0.0.1:8090/v1/context/current
curl http://127.0.0.1:8090/v1/events
```

`/health` 返回与当前状态相同的健康快照，HTTP 200 表示 API 能响应；业务健康需查看 health 字段。事件接口返回最近 100 条状态变化；尚未实现按日、分页、个体过滤或开放事件区间。SQLite 使用独立 `context_events` 表，不迁移旧 Python 身份数据库。无新状态变化时不会写入逐帧记录。默认不保存原始媒体。

API 仅监听 127.0.0.1，单线程处理、有限请求头和 2 秒客户端读写超时，是原型查询接口。公网发布前应替换为成熟 HTTP 服务并加入认证与请求治理。设备地址、账号和运行结果放在被忽略的 `local/` 或用户配置目录，不写入版本库。

工程目录与依赖边界见 [工程分层](project-layout.md)，代码规范和检查命令见 [贡献指引](../../CONTRIBUTING.md)。

板子离线时可先开发和测试 [本地状态核心与回放](local-development.md)。

短期跟踪接口与能力边界见 [主体跟踪](tracking.md)。
