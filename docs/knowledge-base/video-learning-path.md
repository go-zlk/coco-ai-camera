# Jetson 与边缘视觉视频学习路线

整理日期：2026-10-03

这些视频帮助建立平台和产品形态认知。多条教程面向旧版 JetPack，适合学概念，不能直接照抄安装命令到本项目的 JetPack 6.2.1 / TensorRT 10.3 环境。

## 按顺序观看

### 1. 认识开发板

[Getting Started with the Jetson Orin Nano Developer Kit｜NVIDIA Developer](https://www.youtube.com/watch?v=VWdJ4BCtam8)

看：开发板组成、接口、首次启动和典型边缘 AI 用途。视频使用较早期开发套件数据，理解产品形态即可；本项目板端版本见[平台档案](platform-profile.md)。

### 2. 摄像头怎样进入应用

[USB Cameras on NVIDIA Jetson｜JetsonHacks](https://www.youtube.com/watch?v=rs4mQcJAjMM)

看：如何枚举相机能力、使用 V4L2/GStreamer/OpenCV，以及媒体流如何接入程序。它演示 USB 相机，不是本项目 CSI 相机配置教程。GStreamer 的官方概念补充：[Basic tutorial 2: Elements, Pipeline and the Bus](https://gstreamer.freedesktop.org/documentation/tutorials/basic/concepts.html)。

### 3. 视频分析应用由哪些部分构成

[DeepStream SDK for Real-Time Video Analytics｜NVIDIA Developer](https://www.youtube.com/watch?v=59GABT34m4c)

看：视频输入、AI 推理与实时视频分析的组合方式，以及边缘视频分析的应用形态。视频较旧，只学整体架构；运行时版本和功能查看[DeepStream SDK](https://developer.nvidia.com/deepstream-sdk)。

### 4. 视觉模型如何接入 Agent

[Agent Studio：Multimodal VLM + Function-calling LLM｜Jetson AI Lab](https://www.youtube.com/watch?v=9ozwh9EDGhU)

看：VLM 获取视觉上下文后，如何配合语言模型和工具调用。它主要是方向演示，视频中的 JetPack/模型依赖已经过时，不要按其安装步骤操作。

### 5. 实时视频问答的产品形态

[Live VLM WebUI｜Jetson AI Lab Demo](https://www.jetson-ai-lab.com/tutorials/live-vlm-webui/)

看页面里的演示视频，了解摄像头视频与 VLM 问答如何结合。它不是 YouTube 链接；模型大小、内存占用和实时延迟要按 Orin Nano 实测。

### 6. 看一个 Jetson 开发者的真实问题

[Jetson / DeepStream 学习资源讨论｜Reddit](https://www.reddit.com/r/computervision/comments/13867rb/)

这不是教程视频，而是社区学习反馈：Jetson 使用者经常发现媒体管线、DeepStream 和应用代码之间缺少清晰讲解。把它当作开发者体验需求线索，不能当作市场规模或付费证据。

## 观看时记下这四件事

1. 输入源是什么：CSI、USB、RTSP 还是文件？
2. 推理发生在哪：CPU、CUDA/TensorRT、VLM，还是云端？
3. 输出是什么：逐帧框、时间状态、事件、告警、API，还是动作？
4. 它如何处理时间、失败、隐私和设备重启？

多数演示会讲模型效果，却不覆盖长时间状态、断流恢复、状态新鲜度、事件证据和用户数据保留。这些正是我们要在项目里补足和验证的部分。

## 项目可探索方向

| 方向 | 典型产品结果 | 适合本项目的切入点 | 当前顺序 |
|---|---|---|---|
| 本地持续视觉 Context Runtime | 摄像头到结构化状态/事件 API | Jetson、时间状态、低维本地记忆 | 当前主线，先验证客户痛点 |
| 桌面 Context 参考应用 | 在席、工作段、姿态趋势与时间线 | 快速验证端到端闭环 | 当前第一个样例 |
| 垂直边缘视觉 | 工业、零售、实验室等具体工作流 | 选择一个有明确买方和验收指标的事件 | SDK 访谈无信号时优先转向 |
| 机器人感知与动作 | 感知结果驱动导航/操作 | 后续学习 ROS 2/Isaac 生态 | 需要额外机器人平台和应用领域 |
| 本地 VLM 助手 | 用户主动提问时理解画面 | 按需分析关键帧，输出给 agent | 先于长期实时 VLM 更务实 |

## 版本敏感提醒

- NVIDIA Getting Started 视频中的 Orin Nano 规格属于发布早期语境。不要用它替代当前板端测得的算力模式和功耗配置。
- DeepStream 教程常涉及不同 JetPack、CUDA、TensorRT 和 GStreamer 版本。项目当前是 JetPack 6.2.1 / TensorRT 10.3。
- GStreamer 官方资料适合学习元素、caps、buffer、pipeline 和 bus 等概念；Jetson 专有插件以板端 `gst-inspect-1.0` 与当前 NVIDIA 文档为准。
