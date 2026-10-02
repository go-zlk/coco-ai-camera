# Physical Context Engine 产品方向

版本：v0.2 · 2026-10-03

策略与执行文档：[创业方向调研](strategy-research-2026-10.md) · [桌面参考应用 PRD](product-requirements-desk-context-mvp.md) · [数据契约](context-contract-v1.md) · [开发任务表](development-backlog.md)

## 产品定义

项目的产品假设是为带摄像头的 AI 设备提供本地、持续、可追溯的现实上下文运行时。它把摄像头流转成有时间、来源、置信度和状态变化的结构化状态，供不同模型和 agent 通过稳定接口查询。

首个验证场景是桌面工作与学习：

> 让 AI 在回答用户之前，先知道用户此刻真实处于什么状态。

摄像头是首个 Context Sensor，Computer Activity 后续可作为另一类 Context Sensor。产品输出不是视频，而是带时间范围、来源、置信度和个人基线偏差的 `UserState` 与领域事件。

```text
Camera + Computer Activity
          ↓
      Perception
          ↓
   Temporal User State
          ↓
   Personal Baseline
          ↓
     Context API
          ↓
          LLM
          ↓
 Advice / Coaching
          ↓
  Outcome Observation
```

桌面姿态/专注应用是 runtime 的首个参考应用，用于验证感知时序和 API；它本身尚未证明可成为有规模的独立业务。长期可扩展到其他摄像头设备和现场工作流。Home Context 与 Personal World Model 暂作长期探索。

## 当前产品假设

设备团队能调用检测器或 VLM，但持续状态、时间语义、事件证据、断流处理和隐私生命周期可能仍要逐个项目重做。若这类重复工作耗时且存在明确预算，提供可复用的设备侧 Context Runtime 可能缩短交付时间。桌面用户是否愿意为姿态/工作节奏应用付费，是另一条需要单独验证的假设。

第一版只描述可观察行为，不进行疾病、心理状态或情绪诊断。

## Milestone 0：两小时连续状态输出

### 输入

- 单路 CSI 摄像头。
- 单用户、固定桌面机位。
- 可选的本机键鼠/前台应用活动，后续单独接入。

### 输出

每 5–30 秒生成一次 `user_state.json`：

```json
{
  "observed_at": "2026-10-02T10:30:00+08:00",
  "presence": "present",
  "continuous_session_seconds": 4080,
  "posture": "forward",
  "forward_head_ratio_20m": 0.37,
  "posture_trend": "declining",
  "head_orientation": "screen",
  "screen_facing_ratio_10m": 0.82,
  "look_away_count_10m": 11,
  "left_desk_count_60m": 0,
  "confidence": 0.86,
  "freshness_seconds": 2.4,
  "source_health": "online"
}
```

Milestone 0 只承诺：

- 是否在桌前。
- 连续在座时间。
- 明显前倾/正常/无法判断。
- 粗粒度头部方向：屏幕、低头、侧向、未知。
- 离座次数。
- 连续运行两小时。

“凝视屏幕”和精细 gaze 需要人脸关键点或专用 head-pose 模型，不能仅凭 COCO 人体关键点做强结论。

## 软件架构

```text
CSI / RTSP
   ↓
Capture Worker
   ↓ latest-frame queue
Person + Pose Inference
   ↓ observations
Feature Extractor
   ↓ presence/posture/head orientation
Temporal Aggregator
   ↓ windows, debounce, session boundaries
Personal Baseline
   ↓ deviation from self
Context Store (SQLite WAL)
   ↓
Context API + Web UI + LLM Adapter
```

### 进程边界

第一阶段继续采用两进程思路：

1. `context-worker`：拥有摄像头和 GPU，输出结构化 observation。
2. `context-service`：聚合时间状态、管理 SQLite、提供 API，并监督 worker。

推理进程崩溃或摄像头断流时，服务仍能返回 `source_health=offline` 和最后更新时间。

## 与现有代码的关系

### 直接复用

- `open_capture()`：CSI、文件和后续 RTSP 输入。
- YOLO/Ultralytics 推理框架与 TensorRT 部署路径。
- 跟踪、状态防抖和轨迹级稳定思想。
- `MemoryStore` 的 SQLite WAL、实体、观测和事件结构。
- `/api/status`、`/api/timeline`、`/api/entities` 的本地 API 基础。
- 摄像头断流、offline 状态、systemd 和性能观测设计。

### 需要替换或扩展

- `cat` detector → `person + pose` detector。
- `CatStateEstimator` → `UserStateAggregator`。
- Coco/Kui gallery → 单用户 profile 与 personal baseline。
- `state_changed` → session、posture、break、source-health 等领域事件。
- 单帧身份分数 → 时间窗口、趋势和数据新鲜度。

### 暂停为扩展能力

- 宠物个体识别精度优化。
- 小米摄像头控制与多摄像头接入。
- 家庭物体位置追踪。
- 生成式回放。
- 自动控制智能家居。

这些代码保留，但不进入近期关键路径。

## UserState 的事实边界

每个字段都必须携带以下语义：

- `value`：当前值。
- `confidence`：模型和时序聚合后的可信度。
- `observed_at`：最后真实观测时间。
- `freshness`：状态是否过期。
- `window`：计算所使用的时间窗口。
- `source`：camera、computer 或融合结果。

无法观察时输出 `unknown`，不沿用过期状态伪装成当前事实。

## 个人基线

统一阈值只用于冷启动。积累足够数据后，系统学习：

- 常见坐姿关键点分布。
- 正常连续工作时长。
- 常见休息周期。
- 不同时间段的活动模式。

异常判断表达为“相对本人近期基线的偏离”，并保留基线版本和样本数量。

## 验证指标

### 感知与状态

- Presence precision / recall。
- 离座和返回事件时间误差。
- 前倾状态与用户人工标注的一致率。
- 状态抖动次数和 unknown 比例。
- 连续运行两小时的断流、崩溃和内存变化。

### 产品体验

- Context Correctness：用户是否认可状态描述。
- Advice Relevance：加入上下文后，建议是否优于无上下文回答。
- Interruption Cost：提示是否打扰。
- 用户是否感到“AI 知道我现在的真实状态”。

## 接下来的实现顺序

1. 增加 Pose 模型运行模式，输出人体关键点 observation。
2. 实现 presence/session 状态机和离座事件。
3. 实现基础 posture 特征与时序窗口。
4. 输出本地 `user_state.json` 并写入 SQLite。
5. 扩展 `/v1/context/current` 与历史趋势 API。
6. 采集两小时数据并人工标注关键片段。
7. 基于误差决定是否增加人脸关键点/head-pose 模型。
8. 最后接入 LLM，做有/无 context 的对照测试。
