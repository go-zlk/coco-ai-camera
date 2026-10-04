# 小米摄像头取流验证

## 当前设备与状态

- 用户设备：小米智能摄像机 4 4K 版，固件 `5.3.2_0845`，中国大陆。
- 内部型号：`chuangmi.camera.079ac1`。2026-10-04 已在 Apple Silicon Mac 上用 go2rtc v1.9.14 取流成功，浏览器实际显示画面，协议为 xiaomi/miss、视频编码 H.265。云台控制、30 分钟稳定性与断网恢复仍待实测。
- Jetson 地址：`192.168.0.111`。2026-10-04 本次部署尝试 SSH 超时，尚未安装或取到画面。
- 使用官方 go2rtc `v1.9.14` Linux ARM64 发布文件，脚本校验官方发布元数据的 SHA-256。

## 先在笔记本验证（Apple Silicon Mac）

Mac 与摄像头连接同一个可互通的局域网，在工程目录执行：

```bash
bash scripts/start_camera_bridge.sh
```

直接打开 <http://127.0.0.1:1984>，按下文 Add → Xiaomi 步骤登录。无需 Jetson 或 SSH 转发。脚本自动选择固定版本的 Mac ARM64 官方压缩包，校验 SHA-256 后启动。关闭运行终端或 Ctrl+C 可停止，重新执行脚本可以恢复；不自动开机启动。

Mac 先验证取流和播放，无需安装 CUDA 或 Jetson 的 PyTorch 环境。Mac 的账号配置保存在 `~/.config/coco-ai-camera/go2rtc.yaml`，不在 Git 内。Mac 取流成功不代表 Jetson 已验证；后续在 Jetson 重新运行脚本、登录并验证解码即可。

### 不保存画面的检测检查

在安装了 ultralytics、opencv-python 的独立 Python 环境中运行：

```bash
python scripts/validate_camera_stream.py --seconds 30 --device cpu
```

该脚本通过本地 RTSP 解码，约每秒抽取一帧运行现有 YOLOv8n 模型，只检测人和猫，不开窗口、不保存图像或视频。输出解码帧数、尺寸、检测次数和读取失败数；“检测到猫的次数”指采样帧数，不是猫的数量或身份。它验证数据链路，不能用于证明识别准确率、低延迟或长期稳定性。读流失败退出并报告，不自动重连；打开和单次读取有超时限制。

## 1. 在 Jetson 启动桥接器

在 Jetson 的工程目录更新代码后执行：

```bash
cd ~/jetson-edge-vision
git pull --ff-only
bash scripts/start_camera_bridge.sh
```

无需 Docker、sudo 或改变摄像头固件。程序前台运行，关闭终端或 Ctrl+C 会停止。配置在用户目录，重复启动不会覆盖账号与视频源设置。首次测试不自动接管端口或已有服务；若提示地址占用，先查明原有服务，不要直接杀进程。

## 2. 在电脑打开管理页面

电脑另开终端，保持下列连接运行：

```bash
ssh -N -o ExitOnForwardFailure=yes -L 11984:127.0.0.1:1984 zlk@192.168.0.111
```

浏览器打开 <http://127.0.0.1:11984>。在 Jetson 桌面本机操作时，可直接打开 <http://127.0.0.1:1984>。

进入 Add → Xiaomi，由用户自行输入小米账号、密码和验证码，按页面提示加载中国大陆（cn）的设备。选择目标摄像头，使用页面生成的地址添加视频源，命名为 `xiaomi_living_room`，保存配置。如需要在 Config 页保存地址，保留原有 api、rtsp、webrtc 监听设置及登录生成的 xiaomi 账号配置。

配置中账号凭据、完整 xiaomi:// 地址、DID 等不要粘贴到聊天或提交 Git。后续排错只需内部型号（如 xiaomi.camera.xxx）、编码格式、分辨率与脱敏错误。每次连接仍需互联网获取密钥，媒体连接需摄像头与 Jetson 局域网互通。

## 3. 验证画面，再连接检测程序

在首页找到视频源，进入 stream 页面尝试 MSE 播放。此配置关闭 WebRTC 监听，SSH 只转发管理页面；浏览器不支持摄像头编码时，预览失败不等于取流失败。

Jetson 本地的视频地址是：

```text
rtsp://127.0.0.1:8554/xiaomi_living_room
```

可在 Jetson 上有桌面显示的终端运行现有检测程序：

```bash
source ~/venvs/pet-edge/bin/activate
cd ~/jetson-edge-vision
python infer.py --source rtsp://127.0.0.1:8554/xiaomi_living_room --model yolov8n.pt --mode live
```

现有 live 模式需要显示环境，纯 SSH 会话不要以显示失败判断视频不兼容。OpenCV 实际 RTSP 解码能力也需要验证。先保持默认画质；如果带宽或解码负载高，再尝试官方支持的 `subtype=sd`，不假定某个数字代表 4K。

## 验收记录

记录开始和结束时间、内部型号、分辨率、编码、实际帧率、断流次数。保持实际消费者（播放器或推理）连接 30 分钟，不能以管理页面开着代替持续取流。然后测试一次摄像头断网再恢复，分别记录桥接器恢复时间和推理程序是否需要重启；当前尚未承诺推理程序自动重连。

取流成功后再安排无人值守服务、自动恢复和云台控制。镜头移动会影响跟踪和活动判断，需在后续状态估计中单独处理。

## 来源

- [Xiaomi 接入文档](https://github.com/AlexxIT/go2rtc/blob/v1.9.14/internal/xiaomi/README.md)
- [固定版本发布](https://github.com/AlexxIT/go2rtc/releases/tag/v1.9.14)
- [型号兼容性讨论](https://github.com/AlexxIT/go2rtc/issues/1982)
