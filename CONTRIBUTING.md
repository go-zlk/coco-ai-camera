# 开发与贡献指引

工程目录和依赖规则以 [工程分层](docs/development/project-layout.md) 为准；构建与模型要求见 [C++ 运行时](docs/development/cpp-runtime.md)。

## C++ 代码规则

- C++17，文件名使用 `snake_case.h` / `snake_case.cc`，类和结构体使用 `PascalCase`。
- 普通函数和方法使用 `PascalCase`，简单访问器使用 `snake_case`；变量使用 `snake_case`，类私有成员末尾加 `_`。
- 头文件使用与项目路径对应的 include guard；能独立包含，并显式包含所需标准库头文件。
- 对外接口位于 `include/coco/<module>/`；只在本模块使用的类优先放实现文件的匿名命名空间，较大的私有声明放 `src/<module>/`。
- 2 空格缩进、100 列、左对齐指针符号；条件和循环使用花括号。实际排版以 `.clang-format` 为准。
- 注释解释约束、资源所有权和原因，不重复代码字面意思；API 注释说明输入、失败和线程语义。
- 首选 RAII、标准容器与明确所有权；原生 C/CUDA 句柄必须有释放路径。不使用全局 `using namespace`。
- 当前允许异常：启动/模型/存储失败向应用边界传播并以失败退出；不能把失败伪装成空检测结果。
- 新状态机规则写可控时钟测试；格式调整不需要增加重复实现的测试。

现有短变量名和局部 SQL/JSON 实现是功能基线，后续修改其模块时逐步提高语义清晰度；不能仅凭自动排版宣称已经通过完整可读性审查。

## 本地检查

格式工具固定为 clang-format 18，建议装在独立工具环境中：

```bash
python3 -m venv /tmp/coco-tools
/tmp/coco-tools/bin/pip install clang-format==18.1.8
CLANG_FORMAT=/tmp/coco-tools/bin/clang-format scripts/format_cpp.sh --fix
CLANG_FORMAT=/tmp/coco-tools/bin/clang-format scripts/format_cpp.sh --check
python3 scripts/check_layers.py
```

CPU 开发和 CI：

```bash
cmake -S . -B build -DCOCO_TENSORRT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

目标设备构建 TensorRT 时使用 `-DCOCO_TENSORRT=ON`。只开发状态与持久化时，可用独立构建目录：

```bash
cmake -S . -B build/domain -DCOCO_BUILD_RUNTIME=OFF
cmake --build build/domain -j2
ctest --test-dir build/domain --output-on-failure
```

后一种配置不会发现或链接 OpenCV/CUDA/TensorRT；仍需要 SQLite 和线程库。CI 验证 CPU 运行时、格式与层依赖，GPU 引擎加载和真实摄像头需要目标硬件验收。

## 提交流程

1. 明确行为变化与所属模块，更新必要的数据契约。
2. 实现后检查格式、依赖边界，运行与改动有关的测试。
3. 检查 diff，不提交账号、设备地址、模型、原始媒体或本地验证日志。
4. 使用 `feat:`、`fix:`、`refactor:`、`docs:` 等明确目的的提交标题。
5. 按项目约定提交并推送。设备同步不删除用户采集数据、模型或配置。

目前已配置 GitHub Actions 检查，但分支保护与必需状态检查需要仓库管理设置，不由工作流文件自动开启。

## 无板端开发

板子离线时使用 `scripts/build_local.sh`，并通过 `coco-replay` 验证状态与事件；完整流程见 [本地开发](docs/development/local-development.md)。所有模拟输出必须明确标记，独立于真实家庭数据。
