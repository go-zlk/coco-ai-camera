# 项目决策索引

本页只记录当前共识的简要结论；详细范围、理由和继续/停止门槛以链接的权威文档为准。

| 决策 | 当前结论 | 权威依据 |
|---|---|---|
| 最终用户 | 普通消费者；产品必须容易安装和使用，解决日常生活中的真实问题 | [消费者产品方向](../product-direction-physical-context-engine.md) |
| 创业方向 | 做消费者 AI 陪伴产品；Jetson Context Runtime 是内部技术底座，暂不作为独立 SDK 销售 | [方向调研](../strategy-research-2026-10.md) |
| 首个场景候选 | 真实猫咪状态驱动桌宠，面向工作时离家的养猫人；需求与留存尚未验证 | [单猫桌宠候选 MVP](../mvp-spec-single-cat.md) |
| 首轮验证 | 12–15 位消费者访谈、5–10 户家庭两周试用、真实付款行为 | [消费者访谈](../consumer-discovery-guide.md) · [开发路线](../consumer-development-roadmap.md) |
| 首个传感器 | 原型使用单路 CSI 摄像头；消费者摄像头兼容能力尚待验证 | [猫咪桌宠候选 MVP](../mvp-spec-single-cat.md) |
| 数据原则 | 默认不保留原始视频；状态/事件需有时间、来源、置信度、新鲜度和模型版本 | [Context 契约](../context-contract-v1.md) |
| Unknown 规则 | 低质量、离开视野或相机断流时输出 unknown/stale/offline；断流不等于用户离开 | [Context 契约](../context-contract-v1.md) |
| LLM 顺序 | 先验证感知状态、陪伴体验和消费者留存；是否接 LLM 由用户问题决定 | [消费者开发路线](../consumer-development-roadmap.md) |
| 市场决策 | 先验证消费者最近发生的真实行为、家庭试用留存和付款，不因演示好看就扩展硬件 | [消费者访谈指南](../consumer-discovery-guide.md) |

## 历史方向

宠物识别和 AI Home Memory 的代码/文档保留为实验资产和长期探索，不属于当前 MVP 的必要路径。详见[文档导航](../README.md)。
