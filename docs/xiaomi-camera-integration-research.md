# 小米摄像头开源接入调研

调研日期：2026-09-20。

## 结论

当前项目应把“视频接入”和“设备控制”拆成两个适配器：

```text
小米摄像头 ── go2rtc ── RTSP/本地视频流 ── Jetson 感知流水线
      │
      └── Xiaomi Home / MIoT ── 属性、动作、场景控制
```

第一候选视频桥接器是 `AlexxIT/go2rtc`。控制层优先使用小米官方 Home Assistant 集成或独立的 MIoT 适配器。具体支持能力取决于摄像头型号和该型号公开的 MIoT Spec。

## 候选项目

### 1. go2rtc（推荐用于视频流）

- 仓库：https://github.com/AlexxIT/go2rtc
- 许可证：MIT。
- 支持 Linux ARM64，可以运行在 Jetson。
- 小米模块支持 `xiaomi/mess`、`xiaomi/legacy`，以及多种 CS2/TUTK P2P 协议。
- 可把小米私有流桥接成 RTSP、WebRTC 等标准输出。
- 支持多账号、多区域、双向语音和多镜头摄像头。
- 每次建立小米摄像头连接时仍需联网取得加密密钥，媒体连接在本地完成。
- 新型号支持相对较好，旧 `legacy/TUTK` 型号可能不稳定。

支持型号表：https://github.com/AlexxIT/go2rtc/issues/1982

适合本项目的用法：go2rtc 只负责协议适配和流桥接，Jetson 应用通过 `rtsp://127.0.0.1:8554/<stream>` 消费视频，不在推理代码中维护小米私有协议。

### 2. Xiaomi Miloco（重要架构参照）

- 仓库：https://github.com/XiaoMi/xiaomi-miloco
- 小米官方开源的全屋智能 AI 方案。
- 已覆盖小米摄像头视频/音频、家庭成员身份、宠物注册、Home Memory、家庭任务、设备和场景控制。
- Python MIoT 层源码可读，摄像头流依赖闭源预编译库 `libmiot_camera_lite`；官方文档列出 Linux aarch64 版本。
- 摄像头拉流使用 PPCS P2P，设备控制通过 MIoT 云端接口/场景执行。
- 主要推理依赖云端多模态模型 API，官方建议常开主机，未针对 Jetson 本地 TensorRT 流水线设计。
- 许可证明确限制非商业用途，未经小米书面授权不能用于开发应用、Web 服务或其他软件。

适合本项目的用法：研究其账号绑定、摄像头生命周期、静默断流看护、设备状态容器和事件反馈设计；不直接复制受限代码进入计划商业化的代码库。

### 3. Xiaomi Home for Home Assistant（推荐用于官方设备控制）

- 仓库：https://github.com/XiaoMi/ha_xiaomi_home
- 小米官方 OAuth 2.0 接入，可读取设备、属性和动作并控制支持的米家设备。
- 摄像头视频流不属于其支持范围，应与 go2rtc 配合。
- 云台、隐私模式、指示灯等能力只有在具体型号的 MIoT Spec 中公开时才能自动生成。

### 4. hass-xiaomi-miot（社区控制备选）

- 仓库：https://github.com/al-one/hass-xiaomi-miot
- 覆盖大量 MIoT 设备，部分摄像头可取得临时流地址、隐私模式或其他属性。
- 兼容性依赖型号、区域、固件和云端接口，适合验证，不宜把未公开云接口作为核心产品依赖。

### 5. Xiaomi-Dafang-Hacks（只适合少量旧型号）

- 仓库：https://github.com/EliasKotlyar/Xiaomi-Dafang-Hacks
- 通过 SD 卡自定义固件提供本地 RTSP、云台、电灯、红外、MQTT 和抓图。
- 需要兼容的旧摄像头及刷机操作，会替换正常启动流程；不适合新款米家摄像头或普通用户安装流程。

## 对本工程的建议

### 第一阶段：只接视频

1. 确认摄像头完整名称和 MIoT 型号，例如 `chuangmi.camera.061a03`。
2. 在 Jetson 上运行 ARM64 go2rtc。
3. 通过 go2rtc WebUI 绑定小米账号并加载摄像头。
4. 将本地 RTSP 地址传给 `infer.py --source <rtsp-url>`。
5. 增加断流重连、流健康状态和 `offline` 事件。

### 第二阶段：有限控制

接入设备的 MIoT Spec，只开放该型号真实支持的动作：隐私模式、开关、云台预置位、指示灯、夜视或双向语音。控制接口与视频接入分离，防止云端控制故障影响本地感知。

### 第三阶段：产品化边界

- Jetson 不保存小米账号明文密码。
- OAuth token、设备 DID 和流密钥不写入 Git。
- go2rtc API 只监听本机或受保护的局域网地址。
- 摄像头不支持时退回 CSI、USB、标准 RTSP/ONVIF 输入，保持感知层不变。

## 下一项必要信息

必须取得用户家中摄像头的完整产品名和型号代码。只有型号代码能判断 go2rtc 的协议、编码格式、画质档位以及 MIoT 是否提供 PTZ/隐私模式等控制能力。
