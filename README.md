# common

## 仓库说明

`common` 是一个多语言工具库和可复用代码片段仓库。它在同一工作区中维护 Python、C++、Go、Rust、.NET 和 Java 的小型可复用能力，并通过 `./dev` 命令封装常见的构建、测试、覆盖率、格式化和安装流程。

此 README 是面向调用方用法文档的导航入口，不展开每个函数或类。详细的可调用 API 索引和最小用法示例由各语言 README 维护。

## 语言索引

| 语言 | 路径 | 内容 |
|---|---|---|
| Python | [./python/](./python/) | 包工具，涵盖序列化、文件、应用辅助、异步 / 并发、DB/Redis/Mongo 作用域、MQ connector、ASGI/FastAPI 上下文、i18n、绘图、环境检查和可复用脚本。 |
| C++ | [./cpp/](./cpp/) | `cpp/include/mental1104` 下的公共头文件，涵盖容器、缓存、日志、JSON 视图、RAII 包装器、并发辅助工具、随机键、语义基类型、stacktrace C API、Redis 锁和网络 / 事件工具。 |
| Go | [./golang/](./golang/) | `github.com/mental1104/common/golang`，包含可复用包含判断辅助函数，以及可运行示例和实验命令。 |
| Rust | [./rust/](./rust/) | `mental1104` crate，包含集合包含判断辅助函数、已排序切片包装器、prelude 导出和通用错误 / 结果别名。 |
| .NET | [./dotnet/](./dotnet/) | `Mental1104` 库工具，目前聚焦可执行文件校验。 |
| Java | [./java/](./java/) | 基于 Maven 的 Flink 演示，以及可复用 Java 包含判断辅助工具。 |

## 文档规则

新增公共函数、类、结构体、枚举、interface、trait、可复用方法、脚本、CLI 入口或包级工具时，必须在同一次变更中更新对应语言 README。

每个新条目必须包含：

1. 类别
2. 名称
3. 简短用途
4. 导入 / include / 包路径
5. 最小用法示例
6. 备注或限制

Python 条目还必须包含 REPL 用法。如果某个符号在代码中是公开的，但预期稳定性不明确，请将其记录为候选项，并在备注中加入 `待复核`。

旧版小写 [readme.md](./readme.md) 保留历史 dev 命令和覆盖率细节。请将此 `README.md` 作为能力索引入口。
