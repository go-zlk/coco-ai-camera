# 端侧视觉推理 · 总体技术架构

> Jetson Orin Nano 上的 edge-vision 应用。三张图说清：**分层结构**、**推理数据流**、**量化构建链路**。
> Mermaid 语法，Obsidian / VSCode / GitHub 打开即渲染。

## 一、分层架构

```mermaid
flowchart TB
    subgraph APP["应用层 · Application"]
        A1["edge-vision 实时检测<br/>infer.py · live / bench"]
        A2["端侧 LLM 助手<br/>（第二张牌 · 规划中）"]
    end

    subgraph RT["推理运行时 · Inference Runtime"]
        R1["ultralytics YOLO"]
        R2["TensorRT Engine<br/>FP16 / INT8"]
        R3["ONNX Runtime"]
        R4["llama.cpp / Ollama"]
    end

    subgraph FW["AI 框架 · Frameworks"]
        F1["CUDA"]
        F2["cuDNN"]
        F3["TensorRT"]
    end

    subgraph MW["系统中间件 · Middleware"]
        M1["GStreamer<br/>NVMM 零拷贝"]
        M2["OpenCV<br/>采集 / 显示"]
        M3["nvarguscamerasrc<br/>CSI 采集"]
    end

    subgraph OS["操作系统 · OS"]
        O1["JetPack 6<br/>Ubuntu 22.04 + L4T"]
    end

    subgraph HW["硬件 · Hardware"]
        H1["Jetson Orin Nano 8GB<br/>Ampere GPU 1024 核<br/>6×Cortex-A78AE<br/>8GB LPDDR5 共享内存"]
        H2["CSI / USB 摄像头"]
        H3["nvpmodel 功率域"]
    end

    subgraph CC["横切关注点 · Cross-cutting"]
        C1["监控<br/>tegrastats / jtop"]
        C2["功率管理<br/>nvpmodel / jetson_clocks"]
        C3["量化链路<br/>pt → onnx → engine"]
    end

    APP --> RT
    RT --> FW
    MW --> OS
    OS --> HW
    RT --> MW

    CC -.-> APP
    CC -.-> HW
```

**读法**：纵向是依赖链（应用 → 推理运行时 → AI 框架 → 系统 → 硬件）；中间件（GStreamer/OpenCV）是应用和运行时共用的 I/O 横层；右侧横切关注点（监控/功率/量化）贯穿始终——这三条正是你"资源受限优化"叙事的来源。

## 二、推理数据流（含计时分段）

```mermaid
flowchart LR
    CAM["摄像头<br/>CSI / USB"] --> CAP["① 采集<br/>GStreamer / cv2<br/>NVMM 零拷贝"]
    CAP --> PRE["② 预处理<br/>letterbox / 归一化"]
    PRE --> INF["③ 推理<br/>TensorRT<br/>GPU compute"]
    INF --> POST["④ 后处理<br/>NMS"]
    POST --> OUT["⑤ 显示 / 输出<br/>FPS 角标 · 存视频"]

    CAP -.-> T1["t_capture"]
    PRE -.-> T2["t_infer<br/>（端到端，含 ②③④）"]
    INF -.-> T2
    POST -.-> T2
    OUT -.-> T3["t_draw"]
```

**读法**：`infer.py --mode bench` 输出的三段就是 `t_capture` / `t_infer` / `t_draw`，闭环 FPS = 1 / 三者之和。更细的 H2D / GPU compute / D2H 来自 `trtexec --avgRuns=200`（`scripts/build_engine.sh`）。

## 三、量化与构建链路

```mermaid
flowchart LR
    PT["YOLOv8.pt<br/>FP32 权重"] -->|"yolo export"| ONNX["yolov8.onnx<br/>FP32"]
    ONNX -->|"trtexec --fp16"| FP16["yolov8_fp16.engine"]
    ONNX -->|"trtexec --int8<br/>（需校准集）"| INT8["yolov8_int8.engine"]
    FP16 -->|"infer.py --model"| RUN["端侧推理"]
    INT8 -->|"infer.py --model"| RUN
    RUN -->|"trtexec --avgRuns=200"| LAT["H2D / compute / D2H<br/>精细延迟"]
```

**读法**：FP32 → FP16 → INT8 三档，最终在 README 对比表里落下"延迟/吞吐/内存/功耗/精度损失"五项数据。INT8 需要校准数据，放第二周（见 `PLAN.md` D8-9）。

## 与整体职业主线的对应

这张板上的架构，正是「嵌入式 × AI」主线里**中轴（端侧 AI）**的物化：

- **底座**（嵌入式系统工程）→ 本图下半部：OS / 硬件 / 内存约束 / 功率域，是你 4 年经验所在。
- **中轴**（端侧 AI）→ 本图中间：CUDA/TensorRT / 量化 / 推理运行时，是你正在补强的。
- **塔尖**（云端 AI + 产品）→ 本图之外：axiom 的多模型 LLM、ai_groupchat 的 Agent，未来会通过"边云协同"接回来。
