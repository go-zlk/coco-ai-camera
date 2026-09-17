# 单猫桌宠：软件架构与开发指引

版本：设计 v0.1，2026-09-06。状态：待实施；本次交付是设计、任务与验收规范，没有新增应用运行代码，也没有连接实体 Jetson 测试。

阅读顺序：本文件 → [任务清单](development-backlog.md) → [交互式实现架构](diagrams/mvp-software-architecture.html) → 按任务编号实施。

## 1. 范围和成功标准

目标：单摄像头、单猫，Jetson 本地推理，网页显示活动/休息/不可见/离线，以及当天时间线。固定摄像头、单一房间、单一光照条件先通过，再加入夜间挑战样本。

范围依据 `mvp-spec-single-cat.md`。本指引细化它的内部 unknown、断线、存储和执行方式。`product-architecture.md` 中九服务、云端、MQTT、OTA、多端客户端等属于长期探索，不作为本期任务；其中 NVENC、零拷贝及性能数字不能直接作为本板的已验证能力。

本期不加入多猫身份、精细睡眠/进食识别、原生客户端、通用米家接入、云端、公网账号系统、自动 OTA、INT8、ROS 或 DeepStream。局域网版本只能验证家中体验，不能证明办公室远程访问体验已经解决；后续用户试用再增加远程访问。

两周按一个人业余投入约 30—40 小时估算，非工期承诺。条件：板子可访问、一条可用摄像头输入、可获得猫的视频。若环境在第 2 天结束仍未跑通，先完成回放版，明确标为回放演示，硬件里程碑顺延。

最终验收：当天时间线持久化；状态确认后到网页更新 P95 ≤ 5 秒（同一局域网，另测识别窗口延迟）；断流 ≤ 10 秒判离线；输入恢复且模型已加载时 ≤ 15 秒恢复有效观察；12 小时稳定性报告；独立测试视频的状态质量报告。全部是计划目标，需实测。

## 2. 当前代码如何迁移

| 文件 | 当前事实 | 实施处理 |
|---|---|---|
| `infer.py` | OpenCV 采集、Ultralytics 推理、绘制和 benchmark | 保留 live/bench 入口，抽出采集和检测适配器，新增服务入口 |
| `infer.py` | `--device` 未传入调用；断流直接退出；始终依赖显示 | 修复参数传递，服务路径不调用 imshow/plot，采集可重连 |
| `infer.py` | NVMM 最后转 CPU BGR；bench 对空结果和预热失败处理不足 | 如实记录拷贝路径，拒绝空样本，使用总帧数/总耗时计算吞吐 |
| `scripts/build_engine.sh` | 实际执行未提供校准的 INT8；部分失败可能被忽略 | 首版只构建 FP16；失败返回非零；对比固定样本输出，不假定任意 engine 都能由同一加载器加载 |
| `requirements.txt` | 两个依赖未锁版本 | 分离基础服务与 Jetson 视觉依赖；按实机验证锁定，不盲装覆盖板端 OpenCV |
| `README.md` | 有未实测的成果叙述和占位表格 | 改为实施状态；实测完成后再填成果 |

## 3. 运行结构：一个服务，两个进程

Jetson 上一个 systemd unit 启动主进程，由主进程创建一个视觉子进程。浏览器从同一个 HTTP 服务取得静态网页。没有独立数据库服务、消息中间件和前端服务器。

**主进程**拥有状态协调器、子进程监督、SQLite、HTTP、WebSocket。API 使用 FastAPI/Uvicorn，单 worker、生产关闭 reload，防止重复创建相机进程和多个状态写入者。多 worker 会创建多个服务进程，见 [FastAPI 官方说明](https://fastapi.tiangolo.com/deployment/server-workers/)。

**视觉子进程**拥有模型及 CUDA 上下文。内部采集线程更新容量为 1 的最新帧槽；推理循环拿最新未处理帧，每次只处理一帧，初始目标 5 次/秒而非硬性 30 FPS。原始视频帧留在该进程，跨进程只传观察结果和健康信息。明确用 spawn 创建进程，并在子进程内部初始化模型；进程边界和启动约束参考 [Python multiprocessing](https://docs.python.org/3/library/multiprocessing.html)，实机实现对照其安装的 Python 版本。

主进程中的 IPC 接收桥用独立线程读取有界队列，再投递到异步协调器。SQLite 写入由专用线程串行处理，不在 API 事件循环中执行阻塞写入；读操作使用独立连接和短事务。协调器一次处理一个观察结果，事务成功后才更新快照和通知客户端。

**为何隔离视觉：**OpenCV 读取或原生推理可能卡住，主进程仍应返回历史和健康状态，并终止重建子进程。子进程被强制结束后重建 IPC 队列，拒收旧 worker generation 消息，避免旧结果污染恢复后的状态。

## 4. 模块与目录（以下均为拟新增）

```text
pet_edge/
  __main__.py             # 服务/回放入口
  config.py               # 配置解析、校验、配置 hash
  contracts.py            # 消息类型、枚举、API schema
  vision/
    capture.py            # CSI/USB/文件适配；采集新鲜度
    detector.py           # Detector 接口、Ultralytics 适配器
    worker.py             # 最新帧槽、推理、健康上报
  domain/
    associator.py          # 单猫框关联、归一化位移、轨迹失效
    state_machine.py      # 纯规则，无 OpenCV/CUDA/数据库依赖
  service/
    coordinator.py        # 校验顺序、计时、事务、广播
    supervisor.py         # 视觉进程启停、超时、退避重启
    broadcaster.py        # 快照、心跳、有界客户端队列
  storage/
    repository.py         # 单写入者、查询、恢复
    migrations/001.sql    # events、intervals、runtime_checkpoint
  api/app.py              # lifespan、HTTP、WebSocket、静态目录
web/
  index.html
  app.js                  # 客户端状态、重连、时间线
  styles.css              # CSS/SVG 动画、减少动画偏好
configs/example.json
tests/{unit,integration,fixtures}/
deploy/pet-edge.service
scripts/{collect_env.sh,replay_eval.py,soak_test.py}
data/                     # 本地数据库、私人视频；忽略提交
reports/                  # 脱敏评测报告、版本信息
```

接口约定：`Capture.read()` 返回帧及帧时间；`Detector.detect(frame)` 返回框集合；`StateMachine.observe(observation, now)` / `tick(now, health)` 返回候选转换；`Repository.commit_transition()` 原子写入事件、区间与检查点；`Repository.snapshot()` 返回同版本状态。所有内部持续时间使用注入的单调时钟，以便确定性测试。

## 5. 三条开发路径共用一套业务逻辑

1. **模拟观察**：JSONL 观察数据直接进入协调器，开发 Mac 无 GPU 也能验证规则、API、数据库和动画。要注入框/缺失/健康变化，不能只注入最终状态后声称识别链路通过。
2. **视频回放**：文件经过真实检测器再进入同一协调器。帧时间采用媒体时间；业务逻辑使用回放时钟，不能因加速回放改变休息阈值。文件结束为 end_of_replay，不记成摄像头离线。
3. **摄像头实机**：相同检测、规则、存储及前端，换输入和实时时钟。采集时间是本机收到帧的时间，不能宣称等同于传感器曝光时间。

帧槽满则覆盖旧帧。观察队列建议容量 16，满则舍弃最旧观察并记录序列缺口；健康队列独立、定期上报最新值。序列缺口或推理观察过期时清空连续证据窗口，禁止把缺失时段当作低运动。事件和数据库事务不能采用丢弃策略。

## 6. 状态规则：先可靠表达“知道多少”

保留四种产品状态；内部 unknown 是必要的证据不足状态，网页文案为“正在观察”，不算作四态之一。传输失联另有客户端 stale 标记。

建议配置初值（需在验证集调节，冻结后测试）：

| 配置 | 初值 | 含义 |
|---|---:|---|
| detection_conf | 0.50 | 有效猫检测阈值；非行为概率 |
| inference_target_hz | 5 | 推理采样目标 |
| observation_max_age_s | 2 | 过期观察不用于行为判定 |
| active_window_s | 2 | 近期有位移的确认窗口 |
| resting_window_s | 20 | 持续低位移且猫持续可见 |
| missing_timeout_s | 5 | 新鲜、有效画面中持续未检出 |
| camera_offline_s | 8 | 没有新采集帧 |
| minimum_state_hold_s | 3 | 活动/休息动画防抖，故障不受限制 |
| websocket_heartbeat_s | 2 | 即使业务状态不变也发送 |
| client_stale_s | 6 | 浏览器未收到消息，停止显示“实时” |

按优先级执行：

1. 采集无新帧达到 8 秒 → offline，原因 camera_timeout；优先于行为最短保持时间。
2. 帧仍新鲜但推理无新结果超过 2 秒，或多猫/画面质量无法判断 → unknown，带 reason。不可把推理故障当成“猫不在”。
3. 只有新鲜且有效的推理结果持续未检出猫 5 秒，才 → out_of_view。它表示“未看到猫”，不是证明猫离开房间；遮挡和漏检仍可能产生误判。
4. 可见猫经过 2 秒活动证据确认 → active；经过 20 秒低位移确认 → resting；中间区间保持上一可信状态，但失去有效证据须转 unknown。
5. offline/out_of_view 恢复后清空轨迹，从 unknown 重新积累证据，不沿用离线前的 resting。

关联规则：只选择猫类别，单目标采用 IoU 和归一化中心距离关联；长时间丢失、明显跳变或同时出现多只猫时失效。位移除以框对角线以减少距离影响；对窗口内中心做平滑，设置活动阈值高于休息阈值的滞回区。具体位移阈值由样本确定，避免把任意数字当准确率保障。

框不移动无法可靠识别原地玩耍、舔毛，休息只作为低活动估计。首版置信度字段保存检测分数和规则证据，不能将它展示为“睡眠概率”。画面冻结有些设备仍返回成功帧，单靠 read 成功无法检测，列为专项故障样例；不能以静止画面直接判死机。

## 7. 数据与协议

内部 `Observation`：schema_version、worker_generation、frame_seq、captured_at_utc、captured_mono_ns、inferred_mono_ns、image_size、detections[{bbox_xyxy,score,class_name}]、inference_ms、quality_flags。框坐标规范为原图像素；进入规则时再归一化。内部时间字段不直接当网页时钟。

健康消息：generation、last_capture_mono_ns、last_inference_mono_ns、capture_error、worker_heartbeat、dropped_frames。仅 heartbeat 正常不能证明采集/推理正常。

对外快照示例（设计样例）：

```json
{
  "schema_version": 1,
  "type": "snapshot",
  "boot_id": "boot-example",
  "revision": 42,
  "server_time": "2026-09-06T10:00:00Z",
  "pet_id": "cat-1",
  "camera_id": "living-room",
  "state": "resting",
  "reason": "low_motion_window",
  "state_since": "2026-09-06T09:59:40Z",
  "last_observed_at": "2026-09-06T09:59:59Z",
  "camera_status": "online",
  "inference_status": "ready",
  "persistence_status": "ok",
  "evidence": {"detection_score": 0.91, "window_s": 20},
  "model_version": "model-example",
  "rules_version": "rules-v1"
}
```

`revision` 是数据库已提交事件的递增 ID；无状态变化只更新观察水位和心跳，不生成新状态事件。`boot_id` 区分重启。稳定状态很久也持续发送新鲜度。

| 接口 | 返回/行为 |
|---|---|
| `GET /api/v1/status` | 当前已提交快照和健康状态；启动中 state=unknown |
| `GET /api/v1/timeline?date=2026-09-06` | 配置时区的当天区间、总时长、未知/未观测覆盖率、as_of、revision |
| `GET /health/live` | 主服务能否响应；不以猫可见作为条件 |
| `GET /health/ready` | 配置与数据库可用；视觉故障由字段报告，历史仍可访问 |
| `WS /api/v1/stream` | 连接立即发原子快照，随后发 state_changed 和 heartbeat |
| `GET /` | 本地静态网页，无外部 CDN 依赖 |

时间线日期非法返回 422；内部失败返回通用错误码与 request_id；原始异常仅进本地日志。日期使用设备配置的 `Asia/Shanghai`；客户端显示明确时区，不接受无边界历史查询。

WebSocket 首帧快照与订阅注册由协调器串行完成，解决“HTTP 取快照后再订阅”间的漏事件窗口。客户端只接受较新 revision；发现跳号或 boot_id 改变，重新取快照及当天时间线。无需 MVP 全量事件重放协议。每客户端队列有界，慢客户端断开后重连取最新快照；不得拖住数据库写入。

## 8. 持久化与一天时间线

SQLite 位于板端本地磁盘，WAL、单写入者、短事务、busy_timeout；事务性要求优先，可用 synchronous=FULL 并实测写入成本。WAL 允许读写并行但仍只有一个 writer，且不适用于普通跨机网络文件系统，见 [SQLite 官方说明](https://www.sqlite.org/wal.html)。

三张表：

- `events`：id INTEGER PRIMARY KEY AUTOINCREMENT、event_uuid UNIQUE、boot_id、effective_at_ms、state、reason、evidence_json、model_version、rules_version、config_hash。
- `intervals`：id、start_event_id UNIQUE、state、start_ms、end_ms nullable、reason；约束结束不早于开始，应用层保证同一猫区间不重叠，部分唯一索引保证最多一条未结束区间。
- `runtime_checkpoint`：单行，last_event_id、last_confirmed_at_ms、last_observed_at_ms、boot_id、clean_shutdown。每 2 秒提交，服务恢复可判断哪些时间确实被观察。

状态切换一次事务：插入去重事件 → 关闭上一区间 → 新建区间 → 更新 checkpoint → 提交 → 更新内存快照与广播。区间起点使用确认时刻，不倒填候选开始时刻；完整识别延迟另行报告。重复 event_uuid 返回已有提交结果。

崩溃恢复：上一开放区间仅延伸到最后持久化确认水位；水位到重启之间记 unknown/unobserved，禁止把整段宕机时间补成休息或摄像头离线。正常关闭同样先提交水位、关闭区间再退出。数据损失边界：已提交转换保留，水位间隔内最多约 2 秒无法确认，按未知处理。

当天查询使用半开区间 `[本地00:00, 下一本地00:00)`，转换 UTC 后裁剪区间。跨午夜不用写两条事件；活动/休息/不可见/离线/unknown，加未启动和服务停机的 unobserved 区间，共同覆盖当天截至 as_of 的时间，未来时段不计入分母。活动占比明确分母，可同时显示可见时段占比和全天观测覆盖率。

MVP 只查询当天，默认保留最近 7 个本地日方便排错，每日清理更早已结束区间与关联事件，保留开放区间及它的起始事件，不重置 revision。私人视频不入库，不进 Git。磁盘满时不宣称事件已保存：返回 persistence_status=error、标记数据过期、日志报错；恢复后从已提交水位补 unknown，不能悄悄补写未经保存的休息时长。

规则计时用单调时钟，展示用 UTC；检测系统时钟明显跳变时结束当前可信段，记录 clock_adjustment 并重建映射，不能产生负区间。

## 9. 前端行为与开发顺序

使用原生 HTML/CSS/JavaScript 与简单 SVG/CSS 猫动画，不引入首版构建链。当前状态卡、最后观察时间、当天色带、各状态时长四块内容足够。活动=小幅走动，休息=躺下，不可见=走入门后，离线=连接提示，unknown=观察中。

客户端网络断开时显示“连接中断，最后状态为…”，不向数据库提交 offline。后台标签页回来立即重新取快照；动画按状态过渡，避免每次心跳重播。浏览器初始无数据时不默认休息。

第一条可演示链应在第 3 天出现：模拟观察 → 规则 → 数据库 → API → 文字状态网页；随后做动画和真实视觉接入。这使 GPU 环境问题不会阻塞整个产品开发。

## 10. 开发流程与 AI 协作

每次只选择一个带编号任务：明确输入、输出、验收 → AI 读取关联代码提出局部变更 → 实施 → 针对风险验证 → 人工看 diff → 记录结果和未验证条件 → 再接下一个任务。使用本地 Git 管理小提交；当前是否已有 Git 在实施前检查，未初始化再初始化。发布、推送由用户明确授权，不是本设计的自动动作。

任务完成定义：实现可运行、相关测试通过、异常行为有说明、没有凭空性能结论、任务表填写结果路径。纯规则测试不导入 GPU；Mac 通过不等于板端通过。AI 编写适配器和测试，你负责板端连接、真实摄像头位置、样本标签判断及演示体验确认。

可直接使用的提问模板：

> 按 docs/development-backlog.md 完成任务 Txx。先读取当前实现和 software-development-guide.md 的相关约束；只实现本任务及必要依赖。运行有意义的验证，报告变更、证据和板端尚未验证项。发现规格冲突先指出，不静默扩大范围。

每次决策记在简短 decision log：问题、选择、替代方案、证据、何时重新考虑。例如两进程隔离的依据是原生调用故障边界，不是为了增加技术栈数量。

## 11. 验证矩阵

| 层级 | 必须验证的风险 | 运行位置 |
|---|---|---|
| 规则单元 | 单帧漏检、防抖、阈值边界、离线优先、旧观察、恢复清窗、多猫歧义 | Mac/CI，不安装 CUDA |
| 存储集成 | 重复事件、事务回滚、崩溃水位、跨午夜、保留策略、磁盘写失败 | 临时 SQLite |
| API 集成 | WS 首快照与并发转换、重连、慢客户端、数据库失败 | Mac/CI |
| 回放评测 | 静止、位移、遮挡、空画面、昼夜、原地活动 | 开发机/Jetson |
| 硬件故障 | 拔相机、阻塞 read、杀视觉子进程、杀主进程、重启、恢复 | Jetson |
| 长期运行 | 12h、内存增长、温度、丢帧、状态抖动、恢复次数 | Jetson |

至少采集 12 个独立短片段，共约 20—30 分钟覆盖活动、休息、空画面、遮挡、弱光和原地动作；8 段用于调参，4 段锁定测试，每组要覆盖主要状态。同一次连续拍摄不得拆到两组。此数据量只支撑个人场景可行性，不证明家庭泛化。

人工标注可观察行为和模糊区间。按每秒采样评估每状态 precision/recall、混淆矩阵、unknown 占比，以及每小时误转换次数；离线采用故障注入计时单独评估。建议验收目标：测试集 active/resting F1 各 ≥0.80、out_of_view precision ≥0.90、有效画面 unknown ≤20%，报告样本数和逐片结果，不足时如实标“功能通、识别质量未过”。这些阈值是项目目标，不是现有模型性能。

采集→检测完成、观察→提交、提交→浏览器渲染分别测量。跨设备时钟不一致时禁止简单相减作为精确端到端延迟；用统一时钟的测试客户端或经校准的时钟并报告误差，演示视频作辅助证据。Jetson 使用共享内存，报告 RSS 和整机 RAM/GPU 相关采样，不能把“显存+内存”重复相加。

## 12. 部署和验收操作

先提供 collect_env 脚本记录板型、L4T/JetPack、TensorRT、Python、OpenCV GStreamer 支持、摄像头插件和 nvpmodel 实际模式。不在未确认环境时升级系统 CUDA 或安装任意 PyTorch wheel。

配置存源地址、模型路径、阈值、时区、监听地址、数据库路径；凭据只存本机受限配置，日志脱敏。开发默认监听 127.0.0.1；家庭局域网验收时显式配置局域网监听，不配置路由器端口转发。

计划 service：非 root 专用运行用户，按实际需要授予 video/render 设备权限；Restart=on-failure，KillMode=control-group，设置退出超时和启动频率限制。主进程先发停止信号再 join 子进程，超时终止整个子进程组；每代只允许一个视觉子进程。恢复退避 1、2、4、8 秒，达到上限继续受控重试并暴露健康状态。

以下是拟提供的命令接口，代码实现前不能当成现有可用命令：

```bash
python -m pet_edge --config configs/example.json --source mock
python -m pet_edge --config configs/jetson.json --source replay --video data/sample.mp4
python -m pet_edge --config configs/jetson.json --source camera
pytest tests/unit tests/integration
python scripts/replay_eval.py --manifest data/test-manifest.json
```

部署步骤：冻结已验证依赖及模型 hash → 板端启动回放冒烟测试 → 摄像头模式试运行 → 安装 service → 浏览器验收 → 12h 运行 → 保存 reports。首版手工版本回退，保留上一个应用和模型目录；数据库迁移前用 SQLite backup API 生成一致备份，不在 WAL 活跃时只复制 .db。迁移失败停止启动；此期不做自动升级。

最终演示：活动 → 休息 → 离开视野 → 拔相机 → 恢复 → 刷新网页 → 重启服务 → 查看历史。每个阶段展示状态与证据，最后提交录屏、独立评测报告、运行环境和已知限制。
