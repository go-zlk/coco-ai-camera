# Jetson Context Runtime 本地知识库

本目录是项目的学习与工程知识入口，用于回答三类问题：板子和软件环境是什么、系统各层如何连接、我们为什么选择当前开发方向。

## 从这里开始

1. [当前平台档案](platform-profile.md)：已知板端型号、JetPack、摄像头、TensorRT 和 Python 环境。
2. [端侧视觉系统地图](system-map.md)：从传感器到 Context API 的整体链路和模块职责。
3. [视频学习路线](video-learning-path.md)：精选视频、对应知识点、过时内容提醒和观看顺序。
4. [术语表](glossary.md)：统一项目里常用的媒体、推理和状态建模术语。
5. [产品与开发决策索引](decisions.md)：当前假设、决策及其权威文档。

## 权威文档

知识库负责解释和导航，产品行为与开发验收以以下文档为准：

- [方向调研与创业决策](../strategy-research-2026-10.md)
- [桌面 Context 参考应用 PRD](../product-requirements-desk-context-mvp.md)
- [Context 数据契约](../context-contract-v1.md)
- [开发任务与门槛](../development-backlog.md)
- [客户问题访谈指南](../customer-discovery-guide.md)

## 维护规则

- 以仓库文档为主，避免把只存在于聊天里的结论当成事实。
- 区分“板端已验证”“用户提供但未复查”“当前假设”“历史经验”。
- 外部资料记录标题、作者/机构、链接、发布日期或访问日期，以及它能支持什么结论。
- 教程的安装步骤要标明 JetPack / TensorRT 版本；不兼容时只保留概念学习价值。
- 需求和创业假设必须链接到访谈或试点证据；没有证据时标为待验证。
- 产品决策变化时先更新权威文档，再更新本知识库的索引和解释。
