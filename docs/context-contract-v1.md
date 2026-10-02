# Physical Context 数据契约 v1

状态：当前执行基线

版本：v1

日期：2026-10-03

## 1. 设计原则

- 感知 observation 与聚合 state 分开。
- 当前事实与历史推断分开。
- 每个值都能解释来源、时间窗口和新鲜度。
- `unknown` 是合法状态。
- 摄像头离线不能等价为用户离开。
- 契约不绑定具体 Pose 模型，允许后续替换推理实现。

## 2. PoseObservation

推理 worker 输出的单次结构化观测：

```json
{
  "schema_version": 1,
  "sequence": 1842,
  "captured_at": "2026-10-03T09:30:02.120+08:00",
  "source_id": "desk_camera_0",
  "source_health": "online",
  "person": {
    "visible": true,
    "confidence": 0.94,
    "track_id": 1,
    "bbox_norm": [0.24, 0.08, 0.76, 0.96]
  },
  "keypoints": {
    "nose": [0.50, 0.19, 0.91],
    "left_shoulder": [0.39, 0.39, 0.88],
    "right_shoulder": [0.61, 0.40, 0.90]
  },
  "model": {
    "name": "yolov8n-pose",
    "version": "baseline",
    "input_size": 640
  }
}
```

坐标使用画面归一化坐标。不可见关键点保留名称，置信度为 0 或省略坐标。

## 3. UserState

Context Service 输出的当前状态：

```json
{
  "schema_version": 1,
  "generated_at": "2026-10-03T09:30:05+08:00",
  "source_health": "online",
  "presence": {
    "value": "present",
    "confidence": 0.96,
    "observed_at": "2026-10-03T09:30:02.120+08:00",
    "freshness_seconds": 2.88,
    "window_seconds": 5,
    "source": "camera"
  },
  "session": {
    "status": "active",
    "started_at": "2026-10-03T08:22:05+08:00",
    "continuous_seconds": 4080
  },
  "posture": {
    "value": "forward",
    "confidence": 0.82,
    "observed_at": "2026-10-03T09:30:02.120+08:00",
    "freshness_seconds": 2.88,
    "window_seconds": 20,
    "source": "camera",
    "baseline_deviation": 1.7
  },
  "head_orientation": {
    "value": "screen",
    "confidence": 0.72,
    "observed_at": "2026-10-03T09:30:02.120+08:00",
    "freshness_seconds": 2.88,
    "window_seconds": 10,
    "source": "camera"
  },
  "aggregates": {
    "forward_head_ratio_20m": 0.37,
    "screen_facing_ratio_10m": 0.82,
    "look_away_count_10m": 11,
    "left_desk_count_60m": 0
  },
  "baseline": {
    "status": "cold_start",
    "version": 1,
    "sample_minutes": 34
  }
}
```

## 4. 枚举定义

| 字段 | 允许值 |
|---|---|
| `source_health` | `online`, `stale`, `offline` |
| `presence.value` | `present`, `absent`, `unknown` |
| `session.status` | `active`, `ended`, `unknown` |
| `posture.value` | `normal`, `forward`, `unknown` |
| `head_orientation.value` | `screen`, `down`, `side`, `unknown` |
| `baseline.status` | `cold_start`, `learning`, `ready` |

## 5. 新鲜度规则

- observation 超过 5 秒未更新：`source_health=stale`。
- 连续超过 10 秒未更新：`source_health=offline`。
- `stale/offline` 时，派生状态的 `freshness_seconds` 继续增长。
- API 可以返回最后一个状态，但必须保留过期标记。
- offline 不产生 `user_left_desk`。

具体秒数进入配置文件，以上数值是 Milestone 0 默认值。

## 6. 状态机默认参数

| 参数 | 默认值 | 目的 |
|---|---:|---|
| presence enter votes | 3/5 | 防止单帧误检开始 session |
| presence exit duration | 5 s | 容忍短时漏检 |
| break minimum duration | 30 s | 区分真正离座与弯腰/遮挡 |
| posture window | 20 s | 防止姿态标签抖动 |
| posture event cooldown | 10 min | 避免重复提醒 |
| context publish interval | 5 s | API 与 JSON 更新频率 |

参数需要通过标注数据调整，不作为硬编码常量散落在业务代码中。

## 7. ContextEvent

```json
{
  "event_id": "01J...",
  "event_type": "posture_declining",
  "started_at": "2026-10-03T09:10:00+08:00",
  "ended_at": null,
  "confidence": 0.84,
  "source": "camera",
  "subject_id": "local_user",
  "evidence": {
    "observation_sequence_from": 1520,
    "observation_sequence_to": 1842,
    "raw_media_saved": false
  },
  "metadata": {
    "baseline_deviation": 1.7,
    "window_seconds": 1200
  }
}
```

事件必须可回溯到 observation 序号和模型版本，即使不保存原始画面。

## 8. API v1

### `GET /v1/context/current`

返回当前 `UserState`。无数据时返回 200 和 `presence.value=unknown`，同时标记 source health。

### `GET /v1/context/timeline?from=&to=&type=`

按时间范围和事件类型返回领域事件。

### `GET /v1/context/summary?window=today`

返回聚合统计，不调用 LLM：在座时长、session 数、休息次数、前倾占比和数据覆盖率。

### `GET /v1/health`

返回服务、摄像头、worker、模型和数据库健康状态。

## 9. 持久化策略

- observation 可以按 1–5 秒降采样后保存，避免每帧写库。
- state 每次发布时原子写入 `data/user_state.json`。
- event 在稳定状态边沿写入 SQLite。
- 聚合数据按日保存。
- 原始视频默认不保存。

## 10. 版本演进

- 新字段只能向后兼容添加。
- 枚举增加新值时，旧客户端必须退化为 `unknown`。
- 删除或重命名字段需要升级 `schema_version`。
- 数据库事件记录保存 `schema_version` 和 `model_version`。
