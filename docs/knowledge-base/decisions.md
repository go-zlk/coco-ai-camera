# 项目决策索引

本页只记录当前共识的简要结论；详细范围、理由和继续/停止门槛以链接的权威文档为准。

| 决策 | 当前结论 | 权威依据 |
|---|---|---|
| 创业方向 | 验证 Jetson 优先、本地持续视觉 Context Runtime；目前属于待验证假设 | [方向调研](../strategy-research-2026-10.md) |
| 首个场景 | 桌面 presence/session/posture 作为参考应用，不代表姿态 App 市场已经验证 | [桌面参考应用 PRD](../product-requirements-desk-context-mvp.md) |
| 首个传感器 | 单路 CSI 摄像头；RTSP/小米等输入后置 | [开发任务表](../development-backlog.md) |
| 数据原则 | 默认不保留原始视频；状态/事件需有时间、来源、置信度、新鲜度和模型版本 | [Context 契约](../context-contract-v1.md) |
| Unknown 规则 | 低质量、离开视野或相机断流时输出 unknown/stale/offline；断流不等于用户离开 | [Context 契约](../context-contract-v1.md) |
| LLM 顺序 | 先验证感知、时间状态和 API；之后做有/无 Context 的对照实验 | [开发任务表](../development-backlog.md) |
| 市场决策 | 先访谈和付费试点，不虚构 TAM，也不因演示好看就扩展硬件 | [客户访谈指南](../customer-discovery-guide.md) |

## 历史方向

宠物识别和 AI Home Memory 的代码/文档保留为实验资产和长期探索，不属于当前 MVP 的必要路径。详见[文档导航](../README.md)。
