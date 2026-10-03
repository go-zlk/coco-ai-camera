# 术语表

| 术语 | 项目中的意思 |
|---|---|
| Jetson | NVIDIA 面向边缘 AI、机器人和视觉应用的嵌入式计算平台；本项目使用 Orin Nano Developer Kit。 |
| JetPack | Jetson 软件栈和 SDK 发行版本，包含 Jetson Linux 与 CUDA 等组件。版本会影响驱动、GStreamer 插件、TensorRT 和模型兼容性。 |
| CSI | 摄像头串行接口；本项目的 CSI 相机通过 Argus/GStreamer 工作。 |
| V4L2 | Linux Video4Linux2 摄像头和视频设备接口，常用于 USB/UVC 视频设备。 |
| GStreamer | 由可连接的 elements 组成的媒体处理框架；pipeline 描述采集、转换、推理、编码、显示等数据流。 |
| Element | GStreamer 处理单元；例如 source、converter、sink。 |
| Caps | GStreamer pad 上协商的视频格式、分辨率、帧率和 memory feature。 |
| Buffer | 在媒体 pipeline 中携带视频帧、时间戳和元数据的数据对象。 |
| NVMM | Jetson/NVIDIA GStreamer 插件使用的硬件相关视频内存类型；见到 NVMM 不代表整条链路自动 zero-copy。 |
| Argus | NVIDIA Jetson 的相机采集/ISP 管线之一，`nvarguscamerasrc` 使用 Argus 接 CSI 相机。 |
| TensorRT | NVIDIA 推理 SDK，用于构建和执行针对 NVIDIA GPU 优化的推理 engine。 |
| VLM | 视觉语言模型，可根据图像/视频内容进行语言理解；适合按需语义分析，实时吞吐须测量。 |
| Observation | 一次带采集时间、来源、置信度和模型版本的观测，不等同于长期事实。 |
| State | 在时间窗口内由一个或多个 observation 推导出的当前状态。 |
| Event | 状态发生变化时产生的记录，包含开始/结束时间和证据引用。 |
| Freshness | 当前数据距最后真实观测的时间；数据过期时要显式标记 stale/offline。 |
| Unknown | 视野、质量或置信度不足以判断的合法输出；不应用猜测替代。 |
| Context Runtime | 本项目探索的软件层：把持续传感器输入整理成时间化、可追溯、可查询的状态和事件。 |
