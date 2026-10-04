# Python 研究原型

这里保留迁移前的检测/跟踪、宠物身份采集与匹配、Home Memory 实验代码。它们用于算法探索、数据采集与 C++ 结果对比，不是新的常驻服务入口。

从仓库根目录调用脚本，继续显式指定本地模型和数据路径：

```bash
python prototypes/python/infer.py --help
python prototypes/python/enroll_pet.py --help
python prototypes/python/memory_api.py --help
```

脚本之间的 Python 导入仍在本目录内解析；跟踪配置默认通过脚本位置查找。`requirements.txt` 是历史原型依赖清单，不包含 Jetson 专用 PyTorch 安装方案；不要据此覆盖已经验证的 CUDA/PyTorch 环境。

历史文档中的根目录 `python infer.py` 应改为 `python prototypes/python/infer.py`；其他原型入口同理。C++ API 与 Python Memory API 是独立实现，数据表与功能范围不同。
