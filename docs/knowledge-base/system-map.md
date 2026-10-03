# 端侧视觉 Context 系统地图

## 主数据流

```text
Camera / Sensor
      ↓
Capture and Media Pipeline
      ↓ frames + timestamps + source health
Inference Adapter (Pose / detector / VLM on demand)
      ↓ observations + confidence + model version
Temporal State and Event Engine
      ↓ state transitions + evidence references
Local Store and Retention Controls
      ↓
Context API / MCP Adapter / Reference UI
      ↓
AI application, robot, or device workflow
```

当前在 Jetson 上优先验证一条 CSI 相机链路。桌面 presence/session 是首个参考应用；不代表所有传感器、模型或客户端已经实现。

## 各层做什么

| 层 | 责任 | 关键概念 | 当前验证点 |
|---|---|---|---|
| 传感器 | 产生图像/视频 | CSI、USB/UVC、RTSP | CSI 预览已确认正常 |
| 媒体管线 | 采集、缓冲、格式和时间戳 | GStreamer、caps、NVMM、queue | CSI pipeline 已跑到 sink |
| 推理适配器 | 把帧变成模型结果 | TensorRT、ONNX、Pose、检测 | Pose 基线待实现/验收 |
| Observation | 记录一次可信观测 | source、timestamp、confidence、model version | 定义见数据契约 |
| 时间状态 | 聚合观测、消抖、识别变化 | window、unknown、stale、event | Presence/session 为近期任务 |
| 本地记忆 | 保存低维状态与事件 | SQLite WAL、retention、evidence ref | 复用现有存储基础但需明确 schema |
| API | 给应用/agent提供结构化查询 | current、timeline、summary、health | 数据契约 v1 已定义 |
| 应用 | 展示或消费 context | 网页、agent、设备动作 | 桌面参考 UI 后续实现 |

## 三种“时间信息”必须分开

- **观测时间**：相机帧或传感器样本真实产生的时间。
- **状态时间**：系统认为一个状态开始、持续或结束的时间。
- **生成时间**：API/聚合器产出 JSON 的时间。

状态还需要来源、置信度、新鲜度和计算窗口。若相机断流，最后一次状态可以返回，但要变为 stale/offline；断流不能被误判为用户离开。

## Jetson 上的媒体路径

CSI 摄像头通常通过 Argus/GStreamer，例如 `nvarguscamerasrc` 输出 NV12/NVMM 帧。USB 摄像头一般通过 V4L2/GStreamer，例如 `v4l2src`。之后可进行颜色转换、缩放、推理、编码、显示或事件处理。

```text
CSI → nvarguscamerasrc → NVMM buffer → conversion/preprocess → inference
USB → v4l2src          → system/NVMM buffer → conversion/preprocess → inference
```

不能只看到 GStreamer pipeline 就假定 zero-copy。要确认每个 element 两侧的 caps、memory type 和实际 buffer 映射方式。

## 产品层的核心约束

- 原始视频默认不持久化。
- 事件必须引用 observation 序号、时间范围和模型版本。
- 低置信和视野外是 `unknown`，不编造成事实。
- 先验证持续状态/事件是否准确，再让 LLM 基于 Context 推理。
- 先完成单一场景的可靠闭环，再抽象 SDK；不要预先实现多摄像头、多硬件、云平台和全屋自动化。
