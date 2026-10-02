# Physical Context Engine on Jetson Orin Nano

一个本地优先的视觉 Context Engine：把摄像头流转成带时间、来源、置信度和新鲜度的状态与事件，供 AI 设备和应用查询。桌面工作场景是当前首个参考应用。

> 产品假设：让运行在真实设备上的 AI，不只看懂当前一帧，还能可靠地知道状态如何随时间变化，并能说明依据。

## 当前阶段

项目正在从摄像头/宠物感知实验迁移到 Jetson 本地视觉 Context Runtime 的技术与市场验证。桌面个人上下文 MVP 用来验证状态建模和 API，不代表姿态提醒市场已经验证。

已经验证：

- Jetson Orin Nano、JetPack 6.2.1、TensorRT 10.3。
- CSI 摄像头通过 `nvarguscamerasrc` 稳定输出 1280×720。
- OpenCV GStreamer、PyTorch CUDA 和 Ultralytics 推理可用。
- YOLO 检测、ByteTrack、SQLite WAL 和本地 HTTP API 基础链路。
- 本地 Git/GitHub 与 Jetson 自动同步开发流程。

正在实现：

- 单用户 Person/Pose observation。
- presence、session、posture 和离座的时间状态。
- `user_state.json`、Context SQLite 和 `/v1/context/*` API。
- 两小时稳定运行和固定数据评测。

尚未完成：

- Pose 模型的板端基准。
- Personal Baseline。
- Computer Activity Adapter。
- Context + LLM 对照实验。
- 产品级身份认证和远程访问。

## 产品边界

Milestone 0 只处理可观察、可验证的状态：

- 是否在桌前。
- 连续在座时间。
- 明显前倾、正常或无法判断。
- 粗粒度头部方向。
- 离座次数和近期时间趋势。
- 摄像头、推理和数据新鲜度。

系统不进行疾病、心理或情绪诊断，也不宣称精确 gaze tracking。原始视频默认不保存。

## 目标数据流

```text
CSI / RTSP Camera
        ↓
Capture + Person/Pose Worker
        ↓ PoseObservation
Temporal User State
        ↓
Personal Baseline
        ↓
SQLite + user_state.json
        ↓
Context API
        ↓
LLM / Desk Companion / Evaluation
```

目标输出示例：

```json
{
  "presence": {"value": "present", "confidence": 0.96},
  "session": {"status": "active", "continuous_seconds": 4080},
  "posture": {"value": "forward", "confidence": 0.82},
  "head_orientation": {"value": "screen", "confidence": 0.72},
  "aggregates": {
    "forward_head_ratio_20m": 0.37,
    "left_desk_count_60m": 0
  },
  "source_health": "online"
}
```

## 开发方向

当前任务按以下顺序执行：

1. Pose observation 链路。
2. Presence 和 session 状态机。
3. Posture 特征与时间窗口。
4. Context Store、JSON 和 API。
5. 状态页、标注评测和两小时稳定性。
6. Personal Baseline。
7. Context + LLM A/B 验证。

LLM 接入排在感知正确性验证之后。

## 文档

- [文档导航](docs/README.md)
- [方向调研与创业决策](docs/strategy-research-2026-10.md)
- [AI Desk Companion MVP PRD](docs/product-requirements-desk-context-mvp.md)
- [Physical Context 数据契约 v1](docs/context-contract-v1.md)
- [Physical Context Engine 产品方向](docs/product-direction-physical-context-engine.md)
- [当前开发任务表](docs/development-backlog.md)

## 仓库结构

```text
.
├── infer.py                   # 已有检测/跟踪实验入口
├── memory_store.py            # SQLite WAL 事件存储基础
├── memory_api.py              # 已有本地查询 API 基础
├── state_estimator.py         # 历史状态机实验
├── identity_*.py              # 历史宠物身份实验
├── enroll_pet.py              # 历史宠物注册实验
├── trackers/                  # 跟踪器配置
├── scripts/                   # TensorRT/功耗工具
└── docs/                      # 当前产品文档与历史研究
```

宠物识别、小米摄像头和 Home Memory 文档继续保留，作为长期 Home Context 的实验资产，不进入当前 MVP 关键路径。

## 当前开发环境

板端已确认环境：

```text
NVIDIA Jetson Orin Nano Developer Kit
Jetson Linux R36.4.4
JetPack 6.2.1
TensorRT 10.3
Python 3.10
OpenCV 4.8.0 with GStreamer
PyTorch 2.8.0 with CUDA
Ultralytics 8.4.144
```

模型、TensorRT engine、视频和本地数据不提交到 Git。

## 历史实验运行

现有宠物检测与本地 Memory API 仍可运行，用于回归底层能力；它们不是当前产品演示入口。使用前请阅读历史文档并确认模型、gallery 和数据库路径。

## 许可证与隐私

第三方模型与组件需分别检查许可证。持续视频默认只在 Jetson 本地处理；持久化数据优先使用低维状态、事件和聚合指标。
