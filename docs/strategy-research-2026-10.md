# Physical Context Engine：方向调研与创业决策

研究日期：2026-10-03  
状态：当前产品策略基线  
结论置信度：中低；方向适合进入验证，不代表已证明产品市场匹配

## 决策摘要

建议把项目的创业方向定义为：

> **为带摄像头的 AI 设备提供本地、持续、可追溯的现实上下文运行时。它把摄像头流转成带时间、置信度、来源和状态变化的结构化 Context API，让上层 AI 能查询“正在发生什么、刚才变了什么、依据是什么”。**

先服务正在开发视觉 AI 设备的团队和独立开发者，先在 Jetson 上验证。桌面工作场景作为第一个参考应用，用来打磨人体在席、工作时段、粗姿态变化和事件时间线；它是产品能力的演示与试验台，暂不把“姿态提醒 App”当成已验证的独立创业方向。

目前还没有证据证明这个方向有足够市场、付费意愿或防御性。下一阶段要验证的是：设备团队是否反复遇到“摄像头模型能识别单帧，却缺少可靠的持续状态、事件、隐私和 agent 接口”这个问题，并愿意为缩短开发时间付费。

## 为什么调整产品定位

### 基础视觉和个人上下文正在成为平台能力

Google 已公开把摄像头/屏幕实时分享、个人历史和跨应用数据用于 Gemini 的上下文；2026 年 10 月 1 日发布的 Guided Vision 继续强化实时相机理解和口头引导。[Google Gemini Live with camera and screen sharing](https://blog.google/products-and-platforms/products/gemini/gemini-app-updates-io-2025/) · [Google Personal Intelligence](https://blog.google/innovation-and-ai/products/gemini-app/personal-intelligence/) · [Guided Vision](https://blog.google/innovation-and-ai/products/gemini-app/guided-vision-gemini-live/)

Apple 将 Personal Context、Visual Intelligence、系统级 App Intents 和端侧/私有云模型纳入 Apple Intelligence 开发平台。[Apple Intelligence for developers](https://developer.apple.com/apple-intelligence/) · [Apple Visual Intelligence](https://developer.apple.com/documentation/visualintelligence)

含义：单做“拍一帧后让 VLM 描述”“把用户情况拼到 prompt”或通用聊天记忆，平台厂商很容易覆盖。创业项目不能把模型调用或 prompt 拼接当成核心资产。

### 桌面姿态与专注应用已经存在，单功能难形成壁垒

调研已发现本地 webcam 姿态检测、提醒、校准、趋势统计和专注计时产品，例如 [Posture Reminder AI](https://apps.apple.com/us/app/posture-reminder-ai/id1574005886?mt=12)、[WellDesk](https://apps.apple.com/us/app/posture-focus-welldesk/id6762511863?platform=watch) 和 [NeckHack](https://www.neckhack.com/)，Reddit 上也不断有相似独立开发项目发布。[TARS 社区项目](https://www.reddit.com/r/productivity/comments/1rvixlx/i_built_a_free_app_that_uses_my_webcam_to_track/) · [SpineSpy 社区项目](https://www.reddit.com/r/ProductivityApps/comments/1u1olt6/i_built_a_local-first-macos_menubar_app_for_posture_and_focus/)

社区帖子只能证明供给和兴趣存在，不能证明活跃用户、留存或付费意愿。姿态提醒是可用的参考应用，但需要用差异化问题和真实用户数据证明它值得单独收费。

### 低层视频管线已商品化，时间语义仍需要产品化

NVIDIA DeepStream 已提供 GPU 加速的视频接入、多路流处理、跟踪、模型推理和边云连接能力。[DeepStream SDK](https://developer.nvidia.com/deepstream-sdk)

GitHub 上大量 Jetson 项目也已包含检测、MQTT/HTTP、dashboard、事件告警等常见模块，例如 [Edge-Surveillance-Node](https://github.com/shahriar-ahmed-seam/Edge-Surveillance-Node)、[Edge-Ai-Camera-Analytics-Framework](https://github.com/prajwal816/Edge-Ai-Camera-Analytics-Framework) 和 [EdgeForge-Vision](https://github.com/m7hanan/EdgeForge-Vision)。这些项目的 star、demo 或 README 不是商业需求证据，但说明“把模型接进摄像头并做个页面”不够独特。

可尝试的差异层是低层 pipeline 之上的持续语义状态：观测新鲜度、unknown、时间窗口、状态转移、事件证据、用户授权、删除与保留策略，以及稳定的 agent-facing API。这个差异目前只是产品假设，必须通过集成者访谈和付费试点验证。

## 市场问题与客户假设

### 初始客户

小型 AI 硬件团队、摄像头/传感器设备厂商、边缘 AI 集成商，以及正在做 camera-enabled agent 的独立开发者。实际买方可能是技术负责人或创始人；使用者是嵌入式、计算机视觉和应用开发工程师。

### 待验证的昂贵问题

他们已经能调用 detector 或 VLM，但每个项目仍要重复处理：摄像头和媒体管线、模型推理调度、时间状态平滑、事件建模、断流恢复、本地数据存储、可追溯性、API 和隐私控制。若这些工作只是一次性 glue code，客户不会买；若它持续拖慢设备发布、跨型号适配和可靠性验收，就可能形成预算。

### 早期产品形态

不是通用 VLM 平台，也不是再造 DeepStream。做一个 Jetson 优先、模型和上层 agent 可替换的 SDK/runtime：

```text
Camera / sensor
      ↓
capture + inference adapters
      ↓
observations with time/source/confidence
      ↓
temporal state + event engine
      ↓
local store + retention/privacy controls
      ↓
Context API / MCP adapter / sample UI
```

首版要做到：一个相机源、一个模型适配器、一个时间状态应用、可回放的事件、HTTP/JSON 契约、断流和低置信处理。先把接口和开发者体验做扎实，不建多租户云平台，不开发自有基础模型，不做通用家庭自动化。

## 备选方向比较

评分是当前团队判断，不是外部市场测量；5 分最好。

| 方向 | 用户痛点/付费潜力 | 与现有能力匹配 | 竞争/平台风险 | 验证速度 | 判断 |
|---|---:|---:|---:|---:|---|
| 消费级桌面姿态/专注助手 | 2 | 4 | 1 | 5 | 保留为首个参考应用；不先押注创业规模 |
| 家庭 AI Memory / 摄像头问答 | 3 | 4 | 1 | 2 | 需求直观但平台竞争、隐私和多设备集成重 |
| 通用 Jetson 视频分析平台 | 3 | 5 | 2 | 3 | DeepStream 与开源项目覆盖低层能力，范围过宽 |
| 本地持续视觉 Context Runtime | 4（待验证） | 5 | 3 | 4 | 推荐验证；价值来自时间语义、证据与设备侧可靠性 |
| 工业/零售垂直视觉应用 | 4 | 4 | 3 | 2 | 若 SDK 买方不存在，下一候选是选一个有预算的垂直工作流 |

## 创业判断

### 当前可以主张的优势

- 创始人有相机固件、嵌入式 Linux、通信和 OTA 经验，能处理摄像头产品中模型 demo 常略过的设备可靠性问题。
- 已有 Jetson Orin Nano、CSI 摄像头和检测/跟踪/本地存储/API 实验，可把系统从传感器带到应用接口。
- 有机会围绕“本地连续状态”积累真实时间序列、失败处理和隐私设计，而不只做单帧模型调用。

### 目前不能声称的优势

- 没有确认的付费客户或留存数据。
- 没有独占模型、数据网络效应、分销渠道或明显技术专利壁垒。
- Jetson 是开发平台，不是产品壁垒；DeepStream 能覆盖大部分媒体加速基础设施。
- “local-first”是信任属性，单独不足以建立竞争优势。

因此项目应采用阶段投资：先用可运行的开发者产品与客户访谈证明痛点，再扩展；若集成者不愿付费，及时转向有明确预算的垂直摄像头工作流。

## 阶段目标与继续/停止门槛

### 阶段 0：技术与问题发现（当前，2 周）

交付：Jetson 上稳定的相机 → observation → temporal state/event → 本地 API；桌面 Context 是示例应用。完成至少 12 次问题访谈，其中至少 8 位来自 AI 设备/摄像头/边缘集成团队。访谈只问最近一次实际项目、耗时、替代方案和预算，不先推销方案。

进入下一阶段需同时看到：至少 5 位受访者在最近 12 个月遇到重复的持续状态/事件管线问题；至少 3 位愿意提供实际集成场景或测试设备；至少 2 位愿意讨论付费试点、采购流程或明确的开发预算。未达到时，不把“好点子”当作需求，重新选垂直场景。

### 阶段 1：开发者试用（第 3–6 周）

交付：Jetson SDK/API v0、可复制安装指南、两个示例应用（桌面 presence/session 与一个外部试点场景）、事件回放和资源/准确度报告。目标：3 个设计合作方完成集成；至少 2 个在真实设备上连续使用两周；记录集成工时和每周活跃使用，而不只统计下载量。

继续条件：至少 2 个合作方每周使用，且各自能指出比现有方案节省至少 1 个工程周或解决现有工具无法满足的验收要求；至少 1 个进入有付款金额和时间表的试点。

### 阶段 2：付费试点（第 2–3 个月）

只选择一个客户类型和一类事件/状态，不继续横向扩功能。交付：3–5 个付费试点、明确部署与支持边界、隐私/数据删除策略、稳定的 SDK 版本。衡量部署时长、事件准确度、设备在线率、支持工时、试点转正式合同率。

继续条件：至少 3 个客户付费，且至少 2 个在试点后续费或部署到第二个设备/场景。若用户喜欢演示但不愿付费，停止扩大 SDK 功能，转向客户预算所在的垂直解决方案。

### 阶段 3：可重复销售（第 4–9 个月）

交付：支持一个明确硬件系列/SoC 的生产版本、SDK 版本管理、遥测与远程运维（默认不含原始视频）、文档、授权和支持流程。衡量部署周期、毛利、支持成本、续约/扩容和伙伴带来的销售比例。

只有在相同客户画像重复购买后，才评估自研边缘硬件或定制相机。借鉴影石/拓竹的产品纪律：从具体高频任务、易上手体验和稳定交付开始，硬件投资由真实使用和订单拉动，不由愿景拉动。

## 接下来 14 天的具体动作

1. 保留 G0：在 Orin Nano 上跑通人体 Pose 和标准化 observation，完成 30 分钟持续链路。
2. 把 `PoseObservation` / `UserState` / `ContextEvent` 做成稳定、可回放的数据契约；所有字段带时间、来源、置信度和新鲜度。
3. 实现 presence/session 和离座/返回事件；先做状态正确性与日志，不做提醒和健康建议。
4. 建立一个真实的失败案例集：遮挡、离开、逆光、断流、低置信。输出当前准确率、未知率、状态抖动和资源占用。
5. 按[客户问题访谈指南](customer-discovery-guide.md)联系 12 位潜在设备/集成开发者；记录原话、当前 workaround、工程时间、预算和购买角色。
6. 第 14 天按上述门槛做继续/调整/停止决策。没有访谈和外部使用者之前，不增加硬件外壳、复杂 UI、VLM 或云端多租户。

## 市场证据边界

公开产品发布证明大平台在投入，不证明小团队不能建立垂直业务。App Store 和开源项目证明相似方案供给存在，不证明市场收入。社区帖子是问题线索，不是样本代表性调查。当前没有可靠 TAM/SAM/SOM 数字；在确定客户画像、合同价值和部署数量前，不编造市场规模。

## 参考资料

- Google, [Gemini app updates at I/O 2025](https://blog.google/products-and-platforms/products/gemini/gemini-app-updates-io-2025/), 2025-05-20。
- Google, [Personal Intelligence](https://blog.google/innovation-and-ai/products/gemini-app/personal-intelligence/), 2026-01-14。
- Google, [Guided Vision launches in Gemini Live](https://blog.google/innovation-and-ai/products/gemini-app/guided-vision-gemini-live/), 2026-10-01。
- Apple, [Apple Intelligence developer platform](https://developer.apple.com/apple-intelligence/), accessed 2026-10-03。
- Apple, [Visual Intelligence framework](https://developer.apple.com/documentation/visualintelligence), accessed 2026-10-03。
- NVIDIA, [DeepStream SDK](https://developer.nvidia.com/deepstream-sdk), accessed 2026-10-03。
- NVIDIA, [DeepStream video analytics architecture](https://developer.nvidia.com/blog/building-iva-apps-using-deepstream-5-0-updated-for-ga/)。
- GitHub: [Edge-Surveillance-Node](https://github.com/shahriar-ahmed-seam/Edge-Surveillance-Node), [Edge-Ai-Camera-Analytics-Framework](https://github.com/prajwal816/Edge-Ai-Camera-Analytics-Framework), [EdgeForge-Vision](https://github.com/m7hanan/EdgeForge-Vision), reviewed 2026-10-03。
- App Store: [Posture Reminder AI](https://apps.apple.com/us/app/posture-reminder-ai/id1574005886?mt=12), [WellDesk](https://apps.apple.com/us/app/posture-focus-welldesk/id6762511863?platform=watch), accessed 2026-10-03。
- Community: [TARS webcam focus tracker](https://www.reddit.com/r/productivity/comments/1rvixlx/i_built_a_free_app_that_uses_my_webcam_to_track/), [SpineSpy posture/focus app](https://www.reddit.com/r/ProductivityApps/comments/1u1olt6/i_built_a_local-first_macos_menubar_app_for_posture_and_focus/), accessed 2026-10-03。
