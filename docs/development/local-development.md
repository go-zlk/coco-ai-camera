# 不依赖 Jetson 的本地开发

摄像头、模型执行和状态规则已经分离。板子离线时，仍能在 macOS/Linux 上编译状态、配置、存储和回放模块，不需要 OpenCV、CUDA、TensorRT、模型或摄像头。

## 构建与测试

需要 CMake、C++17 编译器、SQLite 开发库。已有工具可直接运行：

```bash
scripts/build_local.sh
```

脚本只编译本地核心并运行回归测试。可通过 `CMAKE_BIN`、`CTEST_BIN` 指定工具；macOS 存在 Command Line Tools 时，仅对该进程选择其编译器和 SDK，不修改系统开发工具设置。

也可用 CMake 3.21+ 的预设：

```bash
cmake --preset local
cmake --build --preset local
ctest --preset local
```

`cpu` 预设需要 OpenCV；`jetson` 预设需要目标设备 CUDA/TensorRT。常规 `cmake -S/-B` 命令仍支持工程声明的最低 CMake 版本。

## 本地回放

```bash
./build/local/coco-replay \
  --input tests/fixtures/observations.csv \
  --db /tmp/coco-replay.db
```

这是人工编写的模拟观察数据，不是模型推理或家庭记录。输出每行 JSON 都带 `mode=replay`。回放使用虚拟单调时钟，因此 16 秒场景可立即完成，输出 freshness 按虚拟时间计算。

链路与实时服务共用：

```text
实时：Camera → Detector → Observation → ContextEngine → EventStore
回放：CSV ─────────────→ Observation → ContextEngine → EventStore
```

CSV 列：

| 列 | 含义 |
|---|---|
| elapsed_ms | 从回放开始计算的虚拟毫秒数，非递减，最多一天 |
| kind | frame 为一次成功推理观察；tick 为无新帧时的超时检查 |
| person_count / cat_count | 当前观察的类别框数 |
| person_confidence / cat_confidence | 类别最高置信度，0～1 |
| observed_at | 事件展示时间；测试中使用人工 UTC 字符串 |

`tick` 的计数和置信度必须全部为零。CSV 不支持引号、逗号转义和多源混合。解析失败会退出，但此前有效行的事件可能已经写入；调试应使用独立数据库。重复回放到同一数据库会追加事件，不做去重。

模拟数据覆盖：同时可见 → 短暂漏检 → 猫持续不可见 → 视频离线 → 恢复可见。未来可增加身份切换和活动窗口的固定回放场景，用同一核心进行回归。

## 核心契约

- `Observation` 不携带图像，只携带来源、计数、置信度、序号、接收时间和推理耗时。
- `ContextEngine` 单源、单写者；没有设备或存储所有权。
- `Observe` 忽略重复、乱序、未来和超过两秒的旧样本；非法来源或非有限数值会报错。
- `FrameReceived` 将采集心跳与推理抽样间隔分离。`Tick` 在五秒无新帧后输出离线事件。
- 离线不会刷新最后成功观察的时间，也不伪造新观测。恢复在线后，类别状态仍需通过确认窗口。
- 应用层复制快照并负责线程同步；调用者提供停止回调，CLI 入口负责 SIGINT/SIGTERM。
- 配置解析拒绝未知/重复参数、缺失值、数字尾部垃圾以及 NaN/Infinity。

## 后续工作安排

1. 短期主体跟踪已接入，继续进行真实场景评估，不把 track_id 当作真实身份。
2. 为活动/休息增加连续观察规则及固定回放场景。
3. 存储扩展为按日、按主体查询有效观察区间，保留不可见/离线语义。
4. 消费端接入 Context API，呈现状态、更新时间和当天时间线。
5. 板子恢复后验证真实检测质量、推理耗时、重连和长时间运行。

当前回放没有 HTTP 监听或图像渲染；它验证状态和事件链路，不能证明模型精度或 GPU 性能。本次本地核心验证也不能替代改动后的 Jetson 验收。

框输入、双猫遮挡回放和 tracks API 见 [短期主体跟踪](tracking.md)。
