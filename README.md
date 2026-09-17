# Edge Vision on Jetson Orin Nano

> 端侧视觉推理的工程化实践：把一个「跑不动」的检测模型，通过量化 + TensorRT + 功率调优，在 8GB 共享内存的边缘设备上做到实时，并给出**延迟 / 吞吐 / 内存 / 功耗 / 精度损失**五项实测数据。

<!-- 放一张 30 秒 demo GIF（实时检测画面 + FPS 角标 + 边跑边滚 jtop 功耗） -->
![demo](docs/demo.gif)

---

## 一句话价值

> 我不只是「下载模型跑一遍」，而是把它当作一次**资源受限设备上的系统优化**：三段延迟拆开计时、三档量化对比、三档功率模式实测。这正是无人机 / 相机 / 智能硬件设备端最需要的工程能力。

## 为什么这么做（工程方法）

Jetson Orin Nano 只有 **8GB 共享内存**，CPU/GPU 抢同一块内存，功耗墙和内存墙同时存在。FP32 的 YOLO 模型直接跑，帧率和功耗都不达标。我做了四件事，每一步都留下数据：

1. **量化**：FP32 → FP16 → INT8，记录精度损失，找到「速度/功耗」和「精度」的平衡点。
2. **TensorRT**：ONNX → engine，用 `trtexec` 拿到 H2D / GPU compute / D2H 三段精细延迟。
3. **功率调优**：`nvpmodel` 三档功率模式 + `jetson_clocks`，实测「降档」是否划算。
4. **端到端闭环**：摄像头采集 → 推理 → 显示，用 Python 循环测真实 FPS（而非只看单帧延迟）。

## 量化对比（待实测填写）

> 用 `python infer.py --mode bench` + `scripts/power.sh` 跑出来，把 `__` 换成真实数字。

| 指标 | FP32 (ONNX) | FP16 (TensorRT) | INT8 (TensorRT) |
|---|---|---|---|
| 单帧推理延迟 GPU (ms) | `__` | `__` | `__` |
| 端到端吞吐 (FPS) | `__` | `__` | `__` |
| 峰值内存 (MB) | `__` | `__` | `__` |
| 整机功耗 (W) | `__` | `__` | `__` |
| mAP50 / 精度损失 | 基准 | `__` | `__` |

**结论一句话（填完数字后写）**：例如「INT8 比 FP32 快 `__` 倍、功耗降 `__`%、精度仅掉 `__`%，在 8GB 内可同时跑 `__` 路流」。

## 环境

- Jetson Orin Nano (8GB) + JetPack 6
- Python 3.10+，CUDA / TensorRT 随 JetPack 自带

```bash
pip install ultralytics opencv-python
sudo nvpmodel -m 0 && sudo jetson_clocks   # 先锁最高性能档
```

## 快速开始

```bash
# 1. 先跑通（PyTorch 权重，不依赖 engine）
python infer.py --source 0 --model yolov8n.pt --mode live

# 1b. 猫咪场景：ByteTrack 保持短时遮挡时的目标轨迹
python infer.py --source csi --model yolov8n.pt --mode live \
  --cat-only --track --imgsz 960 --conf 0.15

# 1c. 自动采集宠物身份样本（每只猫只需运行一次）
python enroll_pet.py --name coco --model yolov8n.pt --source csi

# 1d. 建立 Coco/Kui gallery，并在实时画面中匹配身份
python build_gallery.py --root data/enrollment --output data/gallery.json
python infer.py --source csi --model yolov8n.pt --mode live \
  --cat-only --track --gallery data/gallery.json --identity-threshold 0.55 \
  --db data/home_memory.db

# 在另一个终端启动查询服务（API 不会自行采集画面）
python memory_api.py --db data/home_memory.db --host 0.0.0.0 --port 8080

# 2. 构建 TensorRT engine 并拿到精细延迟（见 scripts/build_engine.sh）
./scripts/build_engine.sh yolov8n

# 3. 用 engine 跑实时
python infer.py --source 0 --model yolov8n_fp16.engine --mode live --save docs/demo.mp4

# 4. 跑基准，出对比表
python infer.py --source test.mp4 --model yolov8n_fp16.engine --mode bench --iters 200
```

- `--source`：`0` = USB 摄像头，`csi` = Jetson CSI 摄像头，或视频文件路径
- `--mode`：`live`（实时 + FPS 角标 + 可选存视频）/ `bench`（固定帧数出均值/中位数/p95）
- `--track`：启用 ByteTrack，缓解转身、短时遮挡造成的连续漏检；目标完全消失后仍由状态机判定为不可见
- `enroll_pet.py`：自动检测、筛选、去重并保存宠物注册样本，不需要逐张拍照
- `build_gallery.py`：从注册样本建立本地身份 gallery；实时匹配失败时显示 `unknown`
- 实时身份标签带轨迹级投票和切换滞后，减少抱猫或遮挡时 Coco/Kui 来回跳变
- `memory_store.py`：本地 SQLite WAL 事件记忆层，保存实体、观测和可查询时间线
- `memory_api.py`：提供 `/api/status`、`/api/timeline`、`/api/entities` 查询接口
- `data/home_memory.db`：第一次带 `--db` 启动推理或 API 时创建；从工程目录运行，避免相对路径落在其他位置

## 项目结构

```
.
├── infer.py                 # 推理脚本：live / bench 两种模式
├── scripts/
│   ├── build_engine.sh      # ONNX 导出 + trtexec 量化 + 精细延迟
│   └── power.sh             # 功率模式切换 + tegrastats 功耗采样
├── README.md
└── PLAN.md                  # 2 周里程碑
```

## 分工说明（数字从哪来）

- **精细 GPU 延迟（H2D / compute / D2H）**：来自 `trtexec --avgRuns=200`，这是「单帧延迟」那一行的权威来源。
- **端到端 FPS**：来自 `infer.py --mode bench`，测的是「采集 + 推理(含前后处理) + 绘制」的真实闭环吞吐，代表实际产品体验。
- **功耗 / 内存**：来自 `scripts/power.sh`（`tegrastats` 每秒采样），推理全程取峰值/均值。

## Roadmap

- [ ] INT8 校准（当前 `build_engine.sh` 先只做 FP16）
- [ ] DeepStream 多路视频流（相机/安防行业硬通货）
- [ ] 一个端侧 LLM（Qwen2.5-3B / Llama-3.2-3B 4-bit）作为第二张牌
