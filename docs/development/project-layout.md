# 工程分层与目录规范

本仓库以 C++17 本地运行时为主线。目录按职责划分，构建按模块划分，依赖通过检查脚本约束。参考 Google C++ Style Guide 的命名、头文件和可读性规则，使用 clang-format 18 自动格式化；C++17、100 列和保留异常是本项目结合 Jetson 环境的选择，不宣称完整采用 Google 内部规范。

## 目录职责

```text
coco-ai-camera/
├── CMakeLists.txt                 # 模块组装、依赖发现、测试与安装
├── .clang-format                 # 唯一 C++ 格式配置
├── .editorconfig                 # 编辑器基础配置
├── CONTRIBUTING.md               # 开发与提交规则
├── apps/context_service/main.cc  # CLI 参数解析、应用启动、退出码
├── include/coco/                 # 公开接口，路径与模块对应
│   ├── application/              # ServiceConfig、应用生命周期接口
│   ├── domain/                   # Context、Event、时间与状态规则
│   ├── media/                    # Frame、Detection 图像数据契约
│   ├── capture/                  # 摄像头采集接口
│   ├── perception/               # 检测接口与 YOLO 处理契约
│   ├── storage/                  # 事件持久化接口
│   └── transport/                # 本地 HTTP 接口
├── src/                          # 与公开接口模块对应的实现
│   ├── application/              # 抽帧调度、模块编排、状态发布
│   ├── domain/                   # 与硬件无关的状态转换
│   ├── capture/                  # RTSP/CSI/USB，缓冲与重连
│   ├── perception/               # ONNX/TensorRT，预处理与后处理
│   ├── storage/                  # SQLite 事件表与查询
│   └── transport/                # HTTP 请求与响应
├── tests/unit/                   # 状态/存储与 YOLO 处理回归
├── cmake/                        # 编译器选项
├── scripts/                      # 格式、分层检查、模型和验证工具
├── prototypes/python/            # Python 研究原型、身份采集与匹配
├── docs/development/             # 现行工程规范和运行指引
├── docs/knowledge-base/          # 学习资料与决策索引
└── .github/workflows/cpp.yml     # Linux CPU 构建、测试和规范检查
```

`build/` 是构建输出；`data/` 是运行数据；`local/` 是个人配置与验证记录。它们均被 Git 忽略。模型和 engine 也不入库，可继续使用本地已有位置，通过参数指定路径。第三方源码若将来需要引入，再建立独立目录和许可证清单，不把下载文件混入业务代码。

## 各层的责任与依赖

| 层 | 负责 | 可以引用的项目层 |
|---|---|---|
| domain | 事件、上下文值、时间窗口、状态确认规则 | domain |
| media | 图像帧、检测框的共享值类型 | domain、media |
| capture | 视频源、最新帧缓冲、重连 | domain、media、capture |
| perception | 模型加载、推理、框解码 | domain、media、perception |
| storage | SQLite 状态变化记录 | domain、storage |
| transport | HTTP 请求解析与查询回调 | domain、transport |
| application | 组合各模块，协调启动、运行和退出 | 所有项目层 |
| apps | 参数解析和启动 application | application |

`domain` 只依赖标准 C++，不引用 OpenCV、TensorRT、SQLite 或系统 socket。`media` 是共享数据契约，包含 OpenCV 值类型，不执行采集或推理。采集不直接调用检测器，检测器不直接写数据库，HTTP 服务不直接调用摄像头。application 完成模块之间的协调。

库目标为 `coco_domain`、`coco_capture`、`coco_perception`、`coco_storage`、`coco_transport`、`coco_application`，提供对应的 `coco::` 别名；`coco_media` 是头文件接口目标。TensorRT/CUDA 依赖限制在 perception 实现。公开业务接口不暴露 TensorRT 对象。

`check_layers.py` 检查显式项目 include 和 domain 的外部依赖；它不是完整的 C++ 语义分析器。CMake 使用逐模块链接声明，新增依赖必须同步更新这份文档与检查规则。

## 线程和资源所有权

- application 创建并拥有采集器、检测器、存储和 HTTP 服务。
- capture 独占 VideoCapture，在采集线程更新单帧邮箱；应用线程取最新帧。
- 检测器由应用线程调用；模型执行上下文与 CUDA stream 不跨线程共享。
- HTTP 线程通过回调读取受互斥锁保护的上下文副本或查询存储。
- storage 用互斥锁保护 SQLite 连接；只持久化状态变化。
- 接收时间与窗口计算用单调时钟；持久化展示时间使用 UTC。

`RunContextService` 当前用于单实例进程，其信号处理属于进程级生命周期。将来嵌入其他应用或支持多实例时，应改为显式停止令牌，不直接复用进程全局信号处理。

## 能力扩展的位置

| 后续能力 | 主要落点 | 输入与输出 |
|---|---|---|
| 目标跟踪 | perception 独立 tracker | Detection → TrackObservation |
| 宠物身份 | perception 独立 identity | 目标裁剪/特征 → 身份候选与置信度 |
| 活动/休息状态 | domain 的时序规则 | 多次观察 → 状态/事件 |
| 多摄像头 | application 调度；capture 源实例 | source_id 隔离的观察与健康状态 |
| 当天时间线 | domain 的区间定义；storage 查询；transport API | 时间范围 → 有效观察区间 |
| 个人基线与长期趋势 | domain 独立模块 | 历史观察 → 基线与偏差 |
| 桌宠网页 | 将来独立 web 客户端 | Context API → 动画与时间线 |
| LLM 上下文 | application 独立用例与外部适配器 | 状态/趋势 → 明确来源的上下文 |

这些是规划接口，当前未实现。不要为每个候选功能预先创建空目录或微服务。每次新增模块，应先确定输入、输出、所有者、失效语义和验证方式。

## 目前保留的技术边界

当前 Context/Event 的 JSON 序列化仍在 domain 的标准库实现中，storage 的最近事件查询返回 JSON 文本。这是保留已有 API 的过渡设计。需要多个展示协议或更复杂查询时，应让 storage 返回事件值列表，把序列化集中到 transport；不把更多协议细节加入状态机。

现阶段是模块化单进程。它还不是插件平台，也没有产品级认证、多摄像头隔离和配置热更新。工程分层为这些能力提供演进边界，不代表它们已经完成。

## 规范参考

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [ClangFormat 配置说明](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
