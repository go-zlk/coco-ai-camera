# Jetson 家庭记忆平台：猫咪 MVP 架构基线

版本 v0.1 · 2026-09-15

这是当前工程的架构基线。长期目标是家庭记忆 AI；当前先让一台 Jetson Orin Nano 在家中观察一只猫，将“可观察状态”可靠地转换成网页桌宠和当天时间线。猫咪是第一个垂直切片，不代表最终产品边界；产品路线见 [product-direction-home-memory.md](product-direction-home-memory.md)。它是目标设计，不代表所有模块已经实现。配套图：[MVP 软件实现架构（Archify）](diagrams/mvp-software-architecture.html)。

## 架构决策

系统采用“端侧推理、主服务编排、网页呈现”的单机形态。Jetson 保存原始视频不出设备，只向客户端发送状态、时间线和健康信息；客户端不直接访问摄像头。

板端只保留两个运行进程：

1. 视觉 worker：拥有摄像头、OpenCV/GStreamer 和模型/CUDA 上下文，只处理最新帧，向主服务发送结构化 observation。
2. 宠物服务：监督 worker，运行状态机、SQLite、HTTP/WebSocket 和静态网页。它是唯一的状态和数据库写入者。

这样做是为了隔离原生摄像头/推理调用的故障边界，同时让 API 和历史记录在视觉 worker 重启时仍可访问。网页首版与 API 使用同一个服务进程，减少部署对象和跨进程协议。

## 数据流

```text
摄像头/回放
  → capture adapter
  → 有界最新帧槽
  → detector
  → observation IPC
  → 状态机（时间窗口、防抖、健康优先级）
  → SQLite 事务
  → snapshot + WebSocket event
  → 网页桌宠和当天时间线
```

视频帧不跨进程传输。帧槽容量为 1，处理不过来时覆盖旧帧；观察消息有界，丢弃时记录序列缺口。状态事件和数据库写入不可静默丢弃。

三种输入模式共用同一套后半链路：模拟 JSONL 先验证业务逻辑，视频回放验证真实 detector，CSI/USB 验证实机采集。回放结束表示 `end_of_replay`，不能伪装成摄像头离线。

## 领域模型

产品状态只有四个：`active`、`resting`、`out_of_view`、`offline`。内部保留 `unknown`，用于证据不足、推理过期、多猫或画面质量异常；浏览器另有 `stale` 表示一段时间没收到服务消息。

状态优先级：采集离线 → 推理过期/证据不足 → 摄像头在线但持续未检测到猫 → 活动/休息规则。单帧漏检不切换状态；离线恢复和不可见恢复都清空旧轨迹，从 `unknown` 重新积累证据。

`resting` 的含义是“持续低位移且猫被看见”，不是睡眠诊断。`out_of_view` 的含义是“当前摄像头没有看到猫”，不是证明猫离开房间。每个状态事件都带 `reason`、置信度、模型版本和规则版本。

## 持久化一致性

SQLite 采用 WAL、单写入者、短事务和 checkpoint。一次状态转换在同一事务内完成：去重事件、关闭旧区间、创建新区间、更新水位；提交成功后才更新内存快照和广播。

服务崩溃时，开放区间只延伸到最后已提交水位；水位之后的空白按未知/未观测处理。当天查询使用本地时区的半开区间，明确显示观测覆盖率和未知时长。数据库满或写入失败时，API 报告持久化异常，不能继续声称时间线已保存。

## API 合同

```text
GET /api/v1/status
GET /api/v1/timeline?date=YYYY-MM-DD
GET /health/live
GET /health/ready
WS  /api/v1/stream
GET /
```

WebSocket 建连后先发送完整快照，再发送带递增 `revision` 的状态变化和心跳。客户端发现 revision 跳号或 boot_id 改变时重新拉取快照。慢客户端断开重连，不得阻塞状态提交。

## 部署和观测

首版使用 systemd 管理主服务，主服务负责 worker 生命周期和指数退避重启；开发环境允许直接启动。API 使用单 worker，避免多个 worker 各自创建相机进程和数据库写入者；是否增加 worker 必须重新评估模型上下文与持久化所有权。[FastAPI 官方部署文档](https://fastapi.tiangolo.com/deployment/server-workers/)

Flight Recorder 至少记录采集新鲜度、推理新鲜度、端到端延迟、P50/P95、处理 FPS、队列丢帧、worker 重启、内存、温度、功耗和模型/规则版本。模型单帧 benchmark 与用户看到的端到端延迟分开报告。

配置、模型和应用版本分离。模型和配置更新先通过回放评测，再在板端冒烟；首版手工切换并保留上一版本，自动 OTA 放到后续阶段。局域网监听默认关闭公网暴露，不上传连续原始视频。

## 实施顺序

1. T01/T02：冻结 JetPack 6.2.1 环境，建立 contracts、配置、模拟输入和注入时钟。
2. T03/T04：完成纯规则状态机和 SQLite repository；先让业务可以脱离 GPU 测试。
3. T05/T06：提供快照、当天时间线、WebSocket 和最小网页桌宠。
4. T07/T08/T09：接入当前 CSI 管线和 YOLO detector，加入 worker、generation、重启与断流处理。
5. T10/T11/T12：冻结独立测试片段，调规则和动画，完成 FP16/端到端基准。
6. T13/T14：systemd、日志、故障注入、12 小时稳定性和完整演示。

详细任务、依赖和验收位于 [development-backlog.md](development-backlog.md)；两周 MVP 边界位于 [mvp-spec-single-cat.md](mvp-spec-single-cat.md)。

## 架构不负责的事情

本基线不承诺多猫 ReID、精细睡眠/进食识别、健康诊断、通用小米协议、云端账号、公网访问、原生手机/手表应用、多摄像头融合、自动 OTA、ROS 2 或 DeepStream。这些需求必须由真实使用数据触发，不能作为首版的隐含依赖。
