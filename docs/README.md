# 文档导航

## 当前执行文档

以下文档共同定义当前开发方向，发生冲突时按顺序以前者为准：

1. [方向调研与创业决策](strategy-research-2026-10.md)
2. [消费者 AI 陪伴产品方向](product-direction-physical-context-engine.md)
3. [真实宠物状态桌宠候选 MVP](mvp-spec-single-cat.md)
4. [消费者产品开发路线](consumer-development-roadmap.md)
5. [消费者问题访谈指南](consumer-discovery-guide.md)

## 可复用技术文档

- [Physical Context 数据契约 v1](context-contract-v1.md)：现有契约以 UserState 为例，字段原则可复用到 PetState。
- [桌面 Context MVP PRD](product-requirements-desk-context-mvp.md)：人体 Pose、时间状态与 API 的技术参考，不是当前消费者产品定义。
- [桌面开发任务表](development-backlog.md)：桌面/Pose 实验任务，正式排期需服从消费者路线的验证门槛。
- [小米摄像头开源接入调研](xiaomi-camera-integration-research.md)
- [软件开发指引](software-development-guide.md)：其中进程隔离、错误处理、SQLite 和部署原则仍可复用；宠物业务示例不是当前需求基线。
- [产品架构拆解](product-architecture.md)：作为长期系统工程参考，不代表近期必须实现的服务数量。

## 本地知识库

- [Jetson Context Runtime 本地知识库](knowledge-base/README.md)：平台档案、系统地图、术语、视频学习路线和项目决策索引。

## 历史实验与长期方向

以下内容保留研究价值，但不进入当前消费者 MVP 关键路径：

- [AI Home Memory 产品方向](product-direction-home-memory.md)
- [宠物应用实现计划](jetson-pet-app-implementation-plan.md)
- [宠物身份识别设计](identity-recognition-design.md)
- [宠物注册体验](enrollment-ux.md)
- [宠物市场调研](pet-companion-market-research-2026-09-04.md)
- [开发者 Runtime 客户访谈指南](customer-discovery-guide.md)：旧 B2B SDK 假设，仅作历史研究记录。
- 旧版架构 HTML 和 Archify 图

已有宠物代码继续保留，用作摄像头、跟踪、状态机、身份 gallery 和本地事件存储的实验资产。

## 工程开发

- [工程分层与目录规范](development/project-layout.md)
- [C++ 运行时构建与运行](development/cpp-runtime.md)
- [开发与贡献指引](../CONTRIBUTING.md)

- [无需 Jetson 的本地开发与观察回放](development/local-development.md)

- [短期主体跟踪与双猫回放](development/tracking.md)

- [单猫四态 POC 验收](development/pet-poc.md)
