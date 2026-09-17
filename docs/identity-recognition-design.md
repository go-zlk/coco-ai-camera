# 个体识别设计：Coco、Kui 与家庭成员

## 目标

当前系统需要区分家里的两只猫：`coco` 和 `kui`，并支持用户像管理门禁成员一样注册、撤销和管理可识别的人员。这里的“身份”分为三种：

```text
检测身份：这一帧看到了猫/人
轨迹身份：连续几帧中的同一个目标（临时 track_id）
个体身份：跨遮挡、跨时间仍属于 Coco/Kui/某位成员
```

`track_id` 不能直接当作个体身份。它只在一次连续轨迹中有效。

## 目前主流的技术组合

### 1. 检测 + 多目标跟踪

YOLO 负责检测，ByteTrack 负责基于运动和框匹配维持短时轨迹。BoT-SORT 在此基础上可以加入外观 ReID，适合目标短暂丢失后重新关联。Ultralytics 当前同时提供 ByteTrack 和支持 ReID 的 BoT-SORT；NVIDIA DeepStream 的 `nvtracker` 也会保存 Re-ID 特征用于目标重新关联。参考：[Ultralytics Track](https://docs.ultralytics.com/modes/track)、[NVIDIA nvtracker](https://docs.nvidia.com/metropolis/deepstream/9.0/text/DS_plugin_gst-nvtracker.html)。

这只能解决“轨迹连续性”，不能可靠回答“这一定是 Coco 还是 Kui”。

### 2. 外观 Embedding + Gallery（推荐的统一方向）

为每个检测框提取一个向量，与已经注册的身份向量库做余弦相似度匹配：

```text
检测框 → crop/alignment → embedding → gallery 检索 → Coco/Kui/某人/unknown
```

人员通常使用人脸检测、对齐和 ArcFace 类 embedding。InsightFace 提供这类检测、对齐和识别组件，但其模型授权要在商业化前单独核对；参考：[InsightFace](https://github.com/deepinsight/insightface)。

猫不能直接套人脸识别。猫更适合使用全身/头部外观 embedding，结合颜色、花纹、体型和机位信息。PetFace 是专门研究动物个体再识别的数据集和基准，可作为后续训练与评估参考；参考：[PetFace](https://github.com/mapooon/PetFace)。

### 3. 固定家庭场景的闭集分类

当前只有 Coco 和 Kui，最实际的第一版是收集每只猫在当前机位下的样本，训练一个两类分类器或 embedding 原型。它比直接使用通用宠物 ReID 模型更容易在 Jetson 上稳定运行，但只能识别已注册的猫，换环境后需要重新采样。

## 推荐架构

```text
Camera
  ↓
Detector（cat/person）
  ↓
Tracker（track_id，短时连续性）
  ↓
Identity Extractor（猫外观 / 人脸 embedding）
  ↓
Identity Registry（gallery + threshold + version）
  ↓
World Model / Timeline
```

统一输出：

```json
{
  "track_id": 7,
  "entity_type": "cat",
  "identity": "coco",
  "identity_status": "matched",
  "similarity": 0.83,
  "confidence": 0.91,
  "timestamp": "2026-09-18T10:00:00+08:00",
  "evidence_id": "frame-00123"
}
```

无法达到阈值时必须输出 `identity: null`、`identity_status: unknown`，不要强行分配给 Coco 或 Kui。

## “门禁式注册”流程

### 注册

1. 用户在网页点击“添加成员/添加宠物”。
2. 输入名称、类型和可选头像。
3. 摄像头采集多张不同角度、光照和距离的样本。
4. 提取 embedding，做质量检查和去重。
5. 保存多个原型向量、阈值、模型版本和创建时间。
6. 用户确认后才加入启用列表。

### 运行时

1. 检测器产生候选框。
2. 跟踪器先尝试沿用当前 `track_id` 的身份。
3. 轨迹中断或置信度下降时，调用 embedding gallery 重新匹配。
4. 相似度低于阈值时标为 `unknown`，进入人工确认队列（可选）。

### 管理

- 支持停用、删除和重新采样。
- 身份向量默认只保存在 Jetson 本地。
- 人脸向量属于敏感生物特征，必须有明确的用户同意、访问控制和删除能力。
- 事件记录保存 `identity_model_version`，模型更换后可以回溯评估。

## 分阶段实现

### P0：先建立身份字段

给当前 observation 增加 `track_id`、`identity`、`identity_status`，继续使用 ByteTrack。暂时只允许手动把一条轨迹标记为 Coco 或 Kui，用来打通时间线和网页展示。

### P1：两只猫的闭集识别

每只猫采集 100–300 个裁剪样本，覆盖正面、侧面、背面、趴卧、遮挡和夜间。先做轻量 embedding/分类器，输出 Coco、Kui、unknown 三类。在当前摄像头和光照下分别评估识别准确率、unknown 拒识率和遮挡后的重新识别率。

### P2：统一身份注册服务

实现 `/identities/enroll`、`/identities/{id}/disable`、`/identities/match` 和 `/identities/{id}/samples`。猫和人共用 Registry 接口，但使用不同的 extractor 和阈值。

### P3：人员人脸注册

接入人脸检测/对齐/embedding；加入活体检测、访问令牌和本地加密存储后，再用于门禁或自动化动作。人员识别失败必须退化为 `unknown_person`，不能触发高风险动作。

## 当前明确不做

- 不把 ByteTrack 的 `track_id` 当作永久身份。
- 不用人脸模型识别猫。
- 不在第一版把识别结果直接用于开门、报警或控制家电。
- 不把未知目标强行归入 Coco、Kui 或某位家庭成员。
- 不上传原始人脸图像或 embedding 到云端作为默认行为。
