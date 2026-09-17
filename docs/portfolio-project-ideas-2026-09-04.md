# 面向 AI 转型的 Jetson 业余项目建议

调研日期：2026-09-04。目标：利用现有 Jetson Orin Nano，以及已有的嵌入式、相机、C/C++、Linux、网络通信与 OTA 经验，形成可运行、可测量、可复现的 AI 工程项目。

## 先给结论

最适合你的组合不是同时启动多个小项目，而是：

1. **旗舰产品：真实宠物状态映射桌宠**——负责业务价值、数据与完整用户体验。
2. **基础设施模块：Edge AI Flight Recorder**——负责稳定性、可观测性、故障复现与设备工程。
3. **实验模块：Edge Model Lab**——负责模型转换、量化、精度、延迟、功耗和回归验证。

三个部分可以在同一条链路中逐步交付，也能各自成为独立仓库或技术文章。最终形成的是一个长期运行、可测量、能迭代的端侧 AI 产品，而不只是一次模型调用。

## 筛选标准

| 标准 | 项目需要证明什么 |
|---|---|
| 真实问题 | 用户、场景、错误成本与取舍是否具体 |
| AI 含量 | 数据、标注、训练/微调、评估、时序推理，而不只调用预训练模型 |
| 端侧工程 | 摄像头、GStreamer、TensorRT、内存、功耗、并发和网络 |
| 产品完整度 | API、UI、历史记录、安装、异常状态和用户反馈 |
| 可复现性 | 环境锁定、自动化脚本、测试样本、指标和失败分析 |
| 完成概率 | 一个人能在业余时间做出阶段性可演示结果 |

## Idea 1：真实宠物状态映射桌宠

**推荐度：★★★★★；建议作为旗舰项目；首个可用版本约 8–12 周业余时间。**

### 问题

家中摄像头通常只提供一段需要主动打开的视频。产品把猫咪的实时状态转成低打扰的桌面陪伴，并提供当天时间线。与普通桌宠相比，动画来源于真实宠物；与普通监控相比，用户不必反复查看视频。

### 技术问题

- CSI、USB、RTSP 及可选 go2rtc 摄像头适配。
- 猫检测、单目标跟踪、区域关系、姿态或短视频行为分类。
- 时间窗口状态机，处理抖动、遮挡、离开画面与断流。
- C++ 常驻推理服务，Python 数据/训练/评估工具。
- 状态事件、当天时间线、桌面客户端和远程同步。
- 真实样本上的准确率、覆盖率、状态延迟、功耗与长期稳定性。

### 作品集证明材料

- 自采且去隐私化的数据说明、标签定义和划分策略。
- 检测、跟踪、行为和端到端状态的分层指标。
- 连续 72 小时或 7 天运行记录，包括断流恢复与资源趋势。
- Jetson 上 PyTorch/ONNX/TensorRT FP16/INT8 对比。
- 一段“真实画面—状态事件—桌宠动画—时间线”同步演示。
- 5–10 户试用记录；明确记录误判和用户关闭产品的原因。

### 可参考项目

- [CBAS](https://github.com/jones-lab-tamu/CBAS)：动物行为视频采集、标注、训练和分析的完整思路；v3 的流式分块处理专门解决大文件内存峰值。
- [Animal Kingdom](https://github.com/sutdcv/Animal-Kingdom)：动物行为理解数据集和动作识别任务，但物种与家用摄像头域不同，不能代替自己的猫咪数据。
- [Cat-Behavior-Monitor](https://github.com/lsyang1111/Cat-Behavior-Monitor)：Jetson 上记录猫出现时间并可视化的早期示例。
- [Frigate](https://github.com/blakeblackshear/frigate)：本地检测、事件、录像和 MQTT 的工程参考。
- [go2rtc Xiaomi 说明](https://github.com/AlexxIT/go2rtc/blob/master/internal/xiaomi/README.md)：部分米家摄像头接入的实验路径。

### 为什么适合你

它同时用到相机、嵌入式 Linux、GPU、网络、长期运行和产品体验。自有宠物场景也使数据获取和持续迭代成为可能。该方向已有相似尝试，因此需要用稳定性、真实性和实测数据形成差异，不能只做一个可爱的动画 Demo。

## Idea 2：Edge AI Flight Recorder

**推荐度：★★★★★；可作为旗舰项目的底座；独立版本约 6–8 周。**

### 问题

许多端侧视觉 Demo 能运行几分钟，却难以回答：为什么三小时后变慢、哪一路摄像头先掉帧、重连后是否泄漏内存、模型升级后错误率是否增加。NVIDIA 社区已有多摄像头分割/检测长时间运行时延迟增长的问题报告，这正是实际设备开发的重要痛点。

### 产品定义

为 Jetson 视觉进程提供轻量“黑匣子”：统一记录每一阶段的延迟、队列深度、丢帧、GPU/CPU/EMC/温度/功耗、摄像头健康、推理版本和关键事件；发生异常时保存异常前后的环形缓冲与配置快照，并支持故障注入和回放。

### 技术问题

- C++ tracing SDK：RAII span、无锁或低竞争事件缓冲、结构化日志。
- `tegrastats`、GStreamer probe 和应用自定义指标的采集适配；实现时以当前 JetPack/工具输出为准。
- Prometheus 指标与 Grafana 仪表盘。
- 摄像头断流、延迟、帧损坏、内存压力、温度降频的故障注入。
- systemd watchdog、进程拉起、退避重连和运行状态机。
- 版本化事件格式与一键导出诊断包，默认清除账号、URL 凭据和原始家庭画面。

### 作品集证明材料

- 24/72 小时稳定性报告和内存斜率。
- 故障注入矩阵：故障、检测时间、恢复时间、是否丢数据。
- 观测开销：开启/关闭 recorder 的 CPU、内存和 P95 延迟差异。
- 一个真实根因案例，例如摄像头断流、队列积压或热降频如何被定位。

### 可参考材料

- [NVIDIA Jetson Platform Services Monitoring](https://docs.nvidia.com/jetson/jps/platform-services/monitoring.html)展示了 CPU、RAM、流 FPS、摄像头状态、Prometheus 和 Grafana 的完整监控方向。
- [Jetson 长期运行延迟增长讨论](https://forums.developer.nvidia.com/t/issues-with-long-term-execution-of-computer-vision-project-using-jetson-orin/368888)说明“跑得起来”和“长期稳定”之间仍有工程空间。
- [jetson-rt-stack](https://github.com/silicondoritos/jetson-rt-stack)公开了验证清单、基准与韧性检查的组织方法。

### 为什么适合你

这是 RTOS/设备稳定性经验迁移到 AI 运行时的直接体现。它本身不是新的模型论文，但对端侧 AI、机器人、智能相机具有清楚的工程价值。若与宠物项目结合，可以用真实故障数据验证可靠性，而不是停留在概念描述。

## Idea 3：Edge Model Lab——模型部署证据生成器

**推荐度：★★★★☆；适合作为持续积累的实验模块；独立版本约 4–6 周。**

### 问题

模型从 PyTorch 到 ONNX/TensorRT 后，常见作品只报告 FPS，遗漏端到端延迟、精度变化、峰值内存、功耗、warm-up、engine 构建条件和实际后端是否生效。不同 JetPack/TensorRT/模型版本还会引入不可移植或构建失败问题。

### 产品定义

给定模型、数据集、精度和功率模式，自动生成带环境指纹的对比报告：精度、单阶段/端到端延迟、吞吐、内存、功耗、温度、构建耗时，以及错误样本对比。每次模型或系统升级自动做回归。

### 必做差异

- 不只包装 `trtexec`；统一采集真实应用链路与模型微基准。
- 验证实际执行后端，避免请求 TensorRT 却回退到 CUDA/CPU。
- 记录 engine 与 JetPack/TensorRT/GPU/shape/calibration 的绑定关系。
- 数据集切分、INT8 校准集和测试集严格分离。
- 生成机器可读 JSON 和可发布 Markdown/HTML 报告。

### 作品集证明材料

- 至少三个模型、三种精度、两种功率模式的可重复结果。
- 误差分析：量化前后哪些样本变化，以及变化原因假设。
- CI 在 x86 做静态/单元验证，Jetson runner 做真实硬件 nightly benchmark。
- 一次 JetPack 或 TensorRT 升级前后的回归报告。

### 可参考材料

- [Jetson Orin Nano benchmarks](https://github.com/hokwangchoi/jetson-orin-nano-benchmarks)已经覆盖视觉、LLM 和 VLM，并记录了内存分配限制与图改写；这说明“再做一张 FPS 表”不足以差异化。
- [NVIDIA jetson_benchmarks](https://github.com/NVIDIA-AI-IOT/jetson_benchmarks)可用作官方基线参考。
- [工业异常检测部署研究](https://github.com/09justin/jetson-anomaly-detection-deployment)展示了 ONNX/CUDA/TensorRT、资源与输出差异的基本结构。

## Idea 4：端侧视觉异常检测盒

**推荐度：★★★★☆；偏工业视觉岗位；约 8–12 周。**

### 场景

选一个自己能稳定拍摄的对象，例如线缆连接、装配件缺失、包装破损、设备指示灯或工作台遗留物。用少量正常样本学习正常模式，再检测异常并生成局部热力图。

### 真正有价值的范围

- 自建一套受控数据采集台，系统改变光线、角度、距离和背景。
- 比较监督检测与无监督/少样本异常检测。
- TensorRT 部署、阈值校准、误报成本和域偏移分析。
- 现场反馈闭环：用户确认误报，进入难例库，离线更新模型。
- 签名模型包、版本检查、升级失败回滚和精度回归。

### 风险

没有真实对象和可重复异常时，项目很容易成为下载 MVTec 数据集后运行模型。选题前必须确认能持续采样。现有 [Jetson 异常检测部署仓库](https://github.com/09justin/jetson-anomaly-detection-deployment)已经完成基础 backend 对比；你的差异应放在真实数据、长期运行、反馈闭环和升级安全上。

## Idea 5：多摄像头宠物连续轨迹与身份保持

**推荐度：★★★★☆；算法难度较高；可作为宠物项目第二阶段；约 10–16 周。**

### 问题

单摄像头有死角，多猫家庭还要区分身份。目标是让宠物在不同摄像头间移动、短时遮挡或离开画面后仍保持身份，并生成“客厅休息 → 厨房进食 → 卧室消失”的轨迹。

### 技术问题

- 单相机跟踪、跨摄像头 ReID、时空约束和轨迹合并。
- 猫咪外观相似、姿态变化、红外画面与遮挡。
- 摄像头时间同步、标定和多流调度。
- 不能只展示成功视频；需报告 ID switch、IDF1/HOTA 和重现后的恢复时间。

### 可参考材料

- [OpenVINO 多摄像头跟踪 Demo](https://github.com/openvinotoolkit/open_model_zoo/blob/master/demos/multi_camera_multi_target_tracking_demo/python/README.md)给出检测、ReID 和身份分配的标准结构。
- [mcreid](https://github.com/matus012/multicam_persistent_id)尤其值得学习其压力测试与明确失败边界，而不是照抄人物模型。
- [MetaWild](https://github.com/ML-4-SocialGood/MetaWild)表明动物 ReID 可以结合时间和环境元数据，但野生动物数据不能直接代表家庭猫咪。

## Idea 6：本地视频语义检索与事件报告

**推荐度：★★★☆☆；适合展示 VLM/LLM 集成；约 8–12 周。**

### 产品

在本地视频事件之上提供自然语言查询，例如“今天猫几点去过水碗附近”“过去三天有没有反复抓门”。系统先用廉价检测/跟踪缩小候选片段，再用小型 VLM 做语义判断，并返回时间点、证据帧和置信度。

### 必须控制的范围

- VLM 只处理事件候选片段，避免 24 小时逐帧推理。
- 回答必须引用具体时间与证据画面；无法确认就输出未知。
- 对固定查询建立标注集，测检索召回和回答正确率。
- 测量 VLM 与视觉管线共存时的内存峰值、首 token 与总响应时间。

### 风险

Orin Nano 8GB 共享内存限制明显。社区已有同时运行 7B 模型与语音组件的个人实验，也有 VLM 摄像头项目；仅把 Ollama 或现成模型启动起来不构成差异。[NVIDIA TensorRT Edge-LLM](https://github.com/NVIDIA/TensorRT-Edge-LLM)提供面向嵌入式 LLM/VLM 的 C++ 推理方向，但仍需用实际 JetPack 与模型组合验证。

## 不建议单独作为作品集主项目

- 直接运行 YOLO 并画框。
- 调用现成聊天模型做“本地 Jarvis”。
- 只做 FP16/INT8 FPS 表，没有精度、功耗与端到端指标。
- 下载公开数据训练火焰、口罩或安全帽检测，缺少自己的场景与错误分析。
- 把多个开源服务写进 Docker Compose 就宣称完成 AI 平台。
- 把 LLM 生成代码数量当成果，而没有自己定义验收标准、数据与实测。

## 推荐实施路线

### 第一阶段：4 周，建立可信基线

完善当前仓库的摄像头采集、ONNX/TensorRT 推理和 benchmark。实现 Edge Model Lab 的最小版本，产出 JSON 与 Markdown 报告。指标至少包括准确率、P50/P95、端到端 FPS、峰值内存、平均/峰值功耗和温度。

### 第二阶段：4–6 周，完成宠物 MVP

实现猫检测与跟踪、休息/活动/未看到/离线四态、事件时间线和桌面显示。录制自己的数据并建立固定验证集。先做局域网内完整链路，再做有鉴权的远程状态同步。

### 第三阶段：4 周，证明设备工程能力

加入 Flight Recorder、systemd 服务、断流重连、队列背压、资源告警与诊断包。进行 72 小时运行和故障注入。记录所有失败和修改前后数据。

### 第四阶段：按反馈选择

- 用户最关心状态准确：做短视频行为模型和个体数据微调。
- 用户最关心全屋覆盖：做多摄像头轨迹与 ReID。
- 用户最关心回顾和查询：做本地 VLM 事件检索。
- 用户无持续兴趣：保留 Flight Recorder 和 Model Lab，转向工业异常检测场景。

## AI 编程工具怎样帮助工程实现

可以让 AI 生成脚手架、配置适配器、测试数据工具、报告页面、文档初稿和重复性绑定代码。需要你自己掌握并能讲清：问题定义、数据标签、模型选择、指标、线程与内存模型、TensorRT 转换、异常状态机、根因分析和技术取舍。

每次让 AI 修改关键代码后，保留设计记录：提出了什么假设、怎样验证、结果是什么、为何接受或否决。项目复盘最有价值的不是代码行数，而是能够拿出失败曲线，解释如何定位并修复问题。

## 最终作品集结构

```text
jetson-edge-vision/
├── apps/pet-companion/       # 旗舰用户场景
├── runtime/                  # C++ capture/inference/event runtime
├── recorder/                 # tracing、metrics、fault capture
├── model-lab/                # export、build、benchmark、regression
├── desktop/                  # 状态展示客户端
├── datasets/README.md        # 数据来源、标签和划分，不提交隐私视频
├── experiments/              # 固定配置与机器可读结果
└── docs/                     # 架构、失败分析、演示与复现步骤
```

项目总结应建立在真实结果上，例如：在 Jetson Orin Nano 上设计并实现多线程视觉事件服务；将模型从 ONNX 部署到 TensorRT；在固定数据集上量化精度、P95 延迟、功耗与内存；连续运行 72 小时并完成断流、热降频和进程异常的检测恢复。没有测出的数字先不要写。
