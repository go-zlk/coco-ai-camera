# 2 周里程碑：把 Orin Nano 变成可展示的端侧 AI 作品

目标：2 周内从「开箱」到「一份带真实数据的 README + 一条 30 秒 demo 视频 + 一条 60 秒讲稿」。

## 第 1 周：先跑通 + 出第一张对比表

| 天 | 做什么 | 交付物 |
|---|---|---|
| D1 | 确认 JetPack 6 / CUDA / TensorRT 可用；装 `ultralytics opencv-python`；`python infer.py --source 0 --model yolov8n.pt --mode live` 跑通 | 屏幕里看到实时检测框 |
| D2–3 | `./scripts/build_engine.sh yolov8n`：导出 ONNX、构建 FP16 engine；改 `--model yolov8n_fp16.engine` 跑 live | 一个 `.engine` 文件 |
| D4–5 | `python infer.py --mode bench` 分别跑 FP32(onnx) 与 FP16(engine)，记下延迟/吞吐 | 对比表前两行数字 |
| D6–7 | `./scripts/power.sh` 切三档功率模式 + `tegrastats` 采样；录第一条 demo GIF | 功耗数字 + 粗版 GIF |

## 第 2 周：补 INT8 + 精度 + 正式对外

| 天 | 做什么 | 交付物 |
|---|---|---|
| D8–9 | INT8 量化：准备 100~500 张真实场景图做校准，补上 `build_engine.sh` 里 INT8 那步 | INT8 engine + 第三行延迟 |
| D10–11 | 精度评估：FP32/FP16/INT8 各跑一遍验证集，算 mAP50 或直观误检对比 | 对比表最后一行（精度损失） |
| D12 | 录正式 30 秒对比视频（FP32 卡顿 vs INT8 流畅分屏，或边跑边滚 jtop 功耗） | `docs/demo.gif` + 录屏 |
| D13–14 | 填 README 真实数字、画架构图、`git init` + push 到 `go-zlk` 设 public、写 60 秒讲稿 | 完整可投递作品 |

## 60 秒讲稿要点（D14 交）

1. 一句话：我在一块 8GB 共享内存的边缘设备上，把一个检测模型做到实时，并测出了五项数据。
2. 三个动作：量化（FP32→FP16→INT8）、TensorRT、功率模式调优。
3. 一个数字：INT8 比 FP32 快 X 倍、功耗降 X%、精度只掉 X%。
4. 一句收尾：这是我无人机/相机设备端经验（资源受限 + 稳定性）在 AI 上的自然延伸。

## 里程碑判断标准（能过才算完成）

- README 对比表里没有 `__`，全是真实数字。
- 有一条能看到 FPS 角标和功耗变化的视频。
- 仓库在 GitHub 是 public，别人 clone 下来能按 README 复现。
- 面试官问「三段延迟分别是什么」，你能不翻笔记说清 H2D / compute / D2H 和端到端 FPS 的区别。
