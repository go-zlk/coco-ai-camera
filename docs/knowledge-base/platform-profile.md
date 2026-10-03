# Jetson Orin Nano 平台档案

最后整理：2026-10-03  
证据状态：来自用户提供的板端命令输出；此页面不是远程实时探测结果

## 硬件与系统

| 项目 | 当前记录 | 状态 |
|---|---|---|
| 开发板 | NVIDIA Jetson Orin Nano Developer Kit | 板端 `/proc/device-tree/model` 已确认 |
| Jetson Linux | R36.4.4 | 板端 `/etc/nv_tegra_release` 已确认 |
| JetPack | 6.2.1 | `nvidia-jetpack` 包已确认 |
| TensorRT | 10.3.0 | `libnvinfer` 包已确认 |
| Python | 3.10.12 | 板端已确认 |
| OpenCV | 4.8.0，包含 GStreamer 支持 | 板端 Python 检查已确认 |
| PyTorch | 2.8.0，CUDA 可用 | 当前虚拟环境检查已确认 |
| Ultralytics | 8.4.144 | 当前虚拟环境检查已确认 |

## 摄像头链路

- CSI 摄像头设备节点 `/dev/video0` 存在，但 CSI 采集实际走 Argus/GStreamer。
- `nvarguscamerasrc` 插件已安装。
- 已确认 CSI 摄像头能按 1280×720、NV12、30 fps caps 输出到 `fakesink`。
- 曾观察到相机传感器模式最高帧率不同于输出 caps；实际帧率/传感器模式要用当前运行日志或性能采样核实。
- 当前用户环境报告说真实画面正常；该确认表示相机预览正确，不等于 Pose 精度或长时间稳定性已验收。

## 当前软件约束

- TensorRT 10 不支持旧版 legacy Caffe 模型；使用 ONNX 或适配当前运行时的模型。
- 网络上的 DeepStream 教程经常基于 JetPack 4/5、TensorRT 8 或 DeepStream 6。概念可学习，安装命令和插件版本不能直接照搬到 JetPack 6.2.1。
- CSI 管线的 `nvarguscamerasrc` 和 USB/UVC 的 `v4l2src` 是不同采集路径。
- Orin Nano 适合本地摄像头推理和轻量多模态实验；持续运行还要测温度、功耗、内存和队列积压，不能从 TOPS 数字推断实际应用 FPS。

## 本地环境信息的边界

板端状态由用户在 2026-10-03 前后提供的命令结果形成。系统更新、重新刷机、虚拟环境重建后，请重新记录版本。除版本变化外，涉及准确率、端到端延迟、温度、功耗、持续运行时长的说法都需要对应实测报告。
