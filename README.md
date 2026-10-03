# 面向消费者的 AI 陪伴产品原型

我们的目标是做一个普通消费者容易上手、能在日常生活中提供陪伴或实际帮助的 AI 产品。当前优先验证“真实猫咪状态驱动桌宠”的消费者体验；Jetson 上的视觉 Context Engine 是研发底座，不是最终要消费者购买的开发平台。

> 产品目标：让 AI 自然融入生活，帮人更轻松地关心和照顾重要的人与事。

## 当前阶段

项目正在从摄像头/宠物感知实验走向消费者产品验证。首个候选场景是上班时通过低打扰桌宠感受家中猫咪状态；访谈和家庭试用尚未完成，因此需求和付费意愿仍待验证。Jetson 开发板仅用于原型，普通用户最终不应接触命令行或开发板配置。

已经验证：

- Jetson Orin Nano、JetPack 6.2.1、TensorRT 10.3。
- CSI 摄像头通过 `nvarguscamerasrc` 稳定输出 1280×720。
- OpenCV GStreamer、PyTorch CUDA 和 Ultralytics 推理可用。
- 猫咪 YOLO 检测、ByteTrack、身份采集/匹配实验、SQLite WAL 和本地 HTTP API 基础链路。
- 本地 Git/GitHub 与 Jetson 自动同步开发流程。

当前下一步：

- 访谈目标养猫人，验证真实需求和桌宠同步带来的增量价值。
- 复用现有猫咪检测链路，完成活动/休息/不可见/离线与当天时间线的消费者原型。
- 把首次设置、隐私、状态可信和第二周持续使用作为验收重点。

尚未完成：

- 消费者访谈、家庭试用和真实付费验证。
- 面向普通用户的独立安装、摄像头兼容、自动恢复与产品级安全。
- 手机端、远程访问和最终计算硬件方案。

## 产品边界

首个猫咪桌宠原型只处理可观察、可验证的状态：

- 猫咪活动中。
- 猫咪休息中（不宣称精确睡眠识别）。
- 摄像头在线但暂时看不到猫。
- 摄像头/推理服务离线或状态过期。
- 时间线只统计有效观察区间；unknown 与离线不伪装成宠物活动。

产品不做疾病、情绪或健康诊断。原始视频默认不保存，状态和观察时间需要明确呈现。

## 目标数据流

```text
CSI / USB Camera
        ↓
Capture + Cat Detection/Tracking
        ↓ PetObservation
Temporal Pet State + Events
        ↓
SQLite Timeline
        ↓
Local API
        ↓
Desktop / Phone Companion
```

目标输出示例：

```json
{
  "pet_state": {"value": "resting", "confidence": 0.82},
  "observed_at": "2026-10-04T10:30:00+08:00",
  "source_health": "online",
  "freshness_seconds": 2.4
}
```

## 开发方向

消费者产品按以下顺序推进：

1. 消费者问题访谈和竞品概念测试。
2. 真实猫咪活动状态同步桌宠原型。
3. 家庭试用：独立设置、状态准确度、低打扰和第二周使用。
4. 真实付款/订金验证。
5. 需求成立后，再扩展设备兼容、手机端和更多生活场景。

## 文档

- [文档导航](docs/README.md)
- [本地知识库入口](docs/knowledge-base/README.md)
- [方向调研与创业决策](docs/strategy-research-2026-10.md)
- [消费者产品开发路线](docs/consumer-development-roadmap.md)
- [消费者问题访谈指南](docs/consumer-discovery-guide.md)
- [真实宠物状态桌宠候选 MVP](docs/mvp-spec-single-cat.md)
- [Physical Context 数据契约 v1](docs/context-contract-v1.md)
- [消费者 AI 陪伴产品方向](docs/product-direction-physical-context-engine.md)

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

桌面人体 Pose 文档作为可复用技术实验保留；小米摄像头与 Home Memory 继续作为后续选项，不进入首个消费者验证周期。

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
