# 短期主体跟踪

C++ 已新增独立 `BoxTracker`，复用于实时感知服务和本地框回放。它不依赖 OpenCV、CUDA 或模型，输入是标准库类型的归一化检测框。

## 链路

```text
Detector → BoxDetection → BoxTracker → TrackSnapshot
                       ↘ 实测计数 → Observation → ContextEngine → API
```

实时服务将检测框从像素转换为归一化坐标。跟踪器用类别隔离、面积比例、预测位置上的 IoU 和中心距离生成候选，然后按匹配分数进行确定性的一对一贪心关联。

运动预测只用于匹配下一次观察。对外的框始终是最后一次实测框；没有把预测框作为新检测结果，也不会用保留轨迹增加人物/猫的当前检测计数。

## 轨迹生命周期

| 状态 | 含义 |
|---|---|
| tentative | 新轨迹，尚未累计两次匹配观察 |
| confirmed | 当前帧匹配成功，累计至少两次观察 |
| lost | 本帧没有匹配，保留最后实测位置与时间，observed=false |
| 移除 | 下一次更新时，距离最后实测已达到三秒；或来源离线后清空 |

三秒内的合理位置恢复可以继续使用原 ID。进程内清空轨迹不会重用已发出的 ID；程序重启后会重新编号。因此标识的范围是 **source_id + 进程会话 + track_id**，不能直接用于跨重启的宠物身份。

`tracks` 是最后一次成功观察的快照。无新帧期间客户端必须结合顶层 `freshness_seconds` 与 `last_seen_age_seconds` 判断有效性；离线状态会清空轨迹。临时轨迹过期检查发生在 tracker 下一次更新时。

## API 扩展

`/v1/context/current` 新增 `tracks` 数组，每项包含：

- `track_id`、`class_id`：会话内轨迹号和类别；人物为 0，猫为 15。
- `status`、`observed`、`confirmed`：生命周期、当前观察是否命中、是否累计通过确认。
- `confidence`：最后实测置信度。
- `bbox`：最后实测的归一化 x/y/width/height。
- `last_seen_age_seconds`：最后实测距当前时间的秒数。

`person_count` / `cat_count` 仍是当前实测框数，不是轨迹总数或独立身份数。SQLite 目前仍保存类别状态变化，不逐帧保存框，也没有持久化轨迹身份。

## 本地验证

```bash
scripts/build_local.sh
./build/local/coco-replay \
  --input tests/fixtures/tracking.csv \
  --db /tmp/coco-tracking-replay.db
```

这是模拟数据，不需要板子。回放程序按 CSV 头自动选择类别观察格式或框格式。框格式为：

```text
elapsed_ms,kind,class_id,confidence,x,y,width,height,observed_at
```

- `detection`：一行一个人物/猫检测框；相同毫秒和 observed_at 的多行合成一帧。
- `empty`：一次在线但没有检测结果的观察；class_id=-1，其他测量值全零。
- `tick`：没有新帧时的超时检查；同样使用零测量占位。
- 不同帧的虚拟时间必须递增；同一时间不能混合 detection 和占位行。

测试场景包括两只猫、检测输入顺序变化、短暂遮挡、无目标观察、视频离线和恢复。单元测试覆盖类别隔离、一对一匹配、过期、重置、乱序时间和非法坐标。

## 当前能力边界

这是几何跟踪功能基线，不是完整 ByteTrack/SORT 实现，也没有外观特征或跨摄像头 ReID。两只相似猫交叉、长时间遮挡、快速大幅移动和摄像头云台转动，仍可能造成 ID 切换。

猫转身后，若还能检测到身体框，或只短暂漏检，就有机会维持轨迹；持续完全漏检超过窗口时，不能凭旧框声称仍看到了猫。

下一步应在真实场景评估 ID 切换率、轨迹碎片率和恢复时间，再决定是否加入成熟关联算法和外观特征。coco/kui 的真实身份需要独立的注册、特征匹配与时序身份稳定策略。
