# Physical Context Engine 开发任务表

状态：桌面人体 Context 技术实验；消费者产品当前路线见 [消费者开发路线](consumer-development-roadmap.md)。本表不再代表当前产品 MVP 排期。

版本：v0.1

日期：2026-10-03

目标是在两周左右完成 Milestone 0：单摄像头、单用户、连续两小时运行，稳定输出当前 UserState、事件时间线和 Context API；同时启动创业方向的客户问题验证。桌面场景是 Context Runtime 的参考应用。

## 阶段门槛

| Gate | 结果 | 通过条件 |
|---|---|---|
| G0 | Pose 链路 | CSI → Pose → 结构化关键点连续运行 30 分钟 |
| G1 | Temporal Context | presence、session、离座和 posture 状态可回放验证 |
| G2 | Context Product | JSON、SQLite、API 和状态页使用同一状态快照 |
| G3 | 质量证据 | 固定标注片段生成指标与失败案例 |
| G4 | Milestone 0 | 两小时稳定性与隐私验收通过 |

任何 Gate 未通过时，先修复当前层，不提前接 LLM。

## P0：当前必须完成

| ID | 任务 | 产物 | 依赖 | 完成证据 |
|---|---|---|---|---|
| C01 | Pose 模型基线 | `context_worker.py` 或独立 pose 模式；输出归一化关键点 | 当前 CSI/Ultralytics 环境 | 30 分钟 observation 日志，无持续积压 |
| C02 | 消息契约 | `PoseObservation`、`UserState`、`ContextEvent` 数据类与 JSON 校验 | 无 | 合法/缺失关键点样本可解析，非法枚举失败 |
| C03 | Presence 状态机 | present/absent/unknown、防抖、stale/offline | C02 | 回放中无单帧误切换；断流不等价离座 |
| C04 | Session 聚合 | session start/end、连续时长、离座/返回计数 | C03 | 人工脚本时间线与输出一致 |
| C05 | Posture 特征 v0 | 肩宽归一化、头肩相对位置、normal/forward/unknown | C01,C02 | 能输出特征和置信度；低质量关键点为 unknown |
| C06 | Temporal Aggregator | 10–20 秒窗口、趋势、事件冷却 | C03,C05 | 稳定坐姿每小时无意义切换 ≤ 3 次 |
| C07 | Context Store | observation 降采样、事件、日聚合、schema/model version | C02,C04,C06 | 重启后历史保留；无逐帧写库 |
| C08 | 原子状态输出 | `data/user_state.json` 每 5 秒原子更新 | C06 | 读取方看不到半写入 JSON |
| C09 | Context API v1 | current、timeline、summary、health | C07,C08 | API 契约与文档一致；stale 状态可见 |
| C10 | 最小状态页 | 当前状态、session、趋势、数据健康 | C09 | 手机/电脑局域网可打开；unknown 表达清楚 |
| C11 | 标注与评测工具 | 固定视频、时间段标签、指标和失败样例 | C01–C06 | Presence/Posture 指标可重复生成 |
| C12 | 两小时稳定性 | 资源采样、断流恢复、隐私检查 | C07–C10 | 报告包含 FPS、P95、内存、温度、事件数 |

## P1：Milestone 0 通过后

| ID | 任务 | 进入条件 |
|---|---|---|
| C13 | 冷启动姿态校准和 Personal Baseline v0 | Posture v0 指标达到门槛 |
| C14 | 粗粒度 head orientation 模型评估 | COCO 关键点无法达到人工一致率时 |
| C15 | Computer Activity Adapter | Camera Context 稳定且字段契约冻结 |
| C16 | Context + LLM A/B 测试 | API 数据覆盖率和正确率通过验收 |
| C17 | 主动建议与反馈记录 | B 组建议相关性稳定优于 A 组 |

## P2：长期扩展

- Home Context 与多传感器融合。
- 小米、RTSP、ONVIF 摄像头适配。
- 宠物/家庭成员身份。
- Home Memory 查询。
- 智能家居动作。
- 端侧小模型或多模态模型。

## 建议日程

### 第 1–2 天：观察链路

- C01、C02。
- 冻结 observation JSON 样例。
- 记录 30 分钟 Pose 数据。

通过 G0 后再实现状态机。

### 第 3–5 天：状态与时间

- C03、C04、C05、C06。
- 使用录制回放调试，避免每次依赖真人坐在镜头前。
- 把 unknown、stale、offline 当成正常路径测试。

### 第 6–8 天：存储与接口

- C07、C08、C09。
- 复用 `MemoryStore`，但迁移到 Context 领域事件。
- 固定时区、schema version 和模型版本。

### 第 9–11 天：状态页与评测

- C10、C11。
- 收集正常坐姿、前倾、离座、遮挡和断流片段。
- 阈值只能根据冻结数据调整。

### 第 12–14 天：稳定性

- C12。
- 修复失败后只重跑受影响的验收。
- 更新 README 中的真实运行状态。

## 当前第一轮开发

下一次代码提交只做 C01 和 C02：

1. 新增 Pose 推理入口，不修改旧宠物模式的行为。
2. 把人体框和关键点转换为 `PoseObservation`。
3. 支持摄像头和视频文件回放。
4. 输出 JSONL observation 日志。
5. 不在这一轮推断 fatigue、focus 或 gaze。

## 并行的产品验证

按 [方向调研与创业决策](strategy-research-2026-10.md) 执行客户问题访谈。访谈不阻塞 C01/C02，但 Milestone 0 完成时必须回看证据：如果没有重复痛点和预算信号，不继续扩大成通用 SDK。

## Definition of Done

每张任务卡必须记录：

```text
用户可观察结果：
输入与配置：
修改文件：
正常路径：
故障路径：
验证命令和结果：
实机运行时长：
指标与失败样例：
仍未验证的内容：
```
