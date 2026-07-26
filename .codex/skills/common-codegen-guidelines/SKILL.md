---
name: common-codegen-guidelines
description: /home/mental1104/code/common 的仓库专用代码生成与验证指南。Codex 在本仓库创建或修改代码、测试、示例、devtool 命令、CI/覆盖率设施、Docker/devops 文件、多语言公共 API、跨语言 ABI 导出、benchmark/性能工作，或更新本仓库技能指南时使用。
---

# Common Codegen Guidelines

Use this skill to make changes that look native to `/home/mental1104/code/common`.
This repository is a multi-language common-library workspace, so generated code must preserve each language's local conventions, devtool integration, and portability guarantees.

## Required Workflow

1. Treat `.codex/skills/common-codegen-guidelines` as the live repository guidance. `AGENTS.md` is only a pointer to the archived guidance.
2. Read the nearest existing implementation, tests, README, build file, and devtool command for the language you are touching. When compatibility may be affected, also inspect the matching `.github/workflows/` workflow and `.github/actions/` local actions.
3. For non-trivial code generation, read `references/repo-style.md`; for historical recent-work context or detailed archived rules, read `references/agents-archive.md`.
4. Keep edits close to the existing module boundary. Prefer extending current helpers, aliases, registries, and command wrappers over adding a parallel style.
5. Add or update tests next to the existing tests for the touched language. Public APIs need behavior tests; cross-language/installation surfaces also need install or verify checks.
6. Validate with `./dev` commands whenever available. Prefer language aliases such as `./dev test-python`, `./dev coverage-cpp`, or `./dev verify-install` over direct tool invocations unless debugging a lower-level failure.
7. Update this skill, especially `references/repo-style.md` or `references/agents-archive.md`, whenever a change creates or changes repository-wide rules, public workflows, install/verify behavior, CI/coverage conventions, or reusable APIs that future agents must know. Do not add new maintenance content to `AGENTS.md`.

## Generation Conditions

Generated code must satisfy these repository conditions:

- **CI matrix is the coding contract**: pull requests do not start GitHub Actions. A push to `main` runs the complete matrix only for affected languages, while a repository-level manual `Main Gate` run expands all language matrices. Code must remain compatible with that complete matrix rather than using Actions as a branch-level debugging loop.
- **Portability first**: support the repository's intended OS/compiler/runtime matrix; for reusable common-library code, treat the lowest CI-supported language/runtime version as the baseline and guard newer language-version features in shared surfaces.
- **Warning-clean**: keep C++/C#/Go/Rust/Java compiler and linter warnings clean because CI and some third-party integrations treat warnings as failures.
- **Local API consistency**: use `mental1104` namespaces/packages/modules and existing helpers for logging, concurrency, database scopes, message queues, coverage extraction, and devtool operations.
- **Validation evidence**: run the smallest meaningful build/test/coverage command, then broaden if the touched surface is shared.
- **No speculative performance claims**: for performance, benchmark, GC, scheduler, lock, contention, latency, or throughput work, follow the mandatory benchmark protocol in `references/agents-archive.md` before explaining conclusions.
- **No unnecessary churn**: do not reformat unrelated files, rewrite generated artifacts, or change CI/Docker layers unless required by the task.

## 公共 API 文档规则

本仓库是多语言 common / utilities 仓库。任何新增或变更的可复用公共能力，都必须同步记录文档。

公共能力包括但不限于：

- 公共函数
- 公共类
- 公共结构体
- 公共枚举
- 公共 interface / trait
- 预期复用的公共方法
- 包级工具
- 可复用脚本
- CLI 入口

新增或修改公共能力时，必须更新对应语言目录的 `README.md`。文档更新应写入语言专属 README，而不只写在源码注释中。

根目录 `README.md` 保持为高层导航页。函数、类、类型和脚本的详细用法应写入语言专属 README，例如：

- `python/README.md`
- `cpp/README.md`
- `golang/README.md`
- `rust/README.md`
- `dotnet/README.md`
- `java/flink-datastream-demo/README.md`

语言 README 中的公共 API 文档默认使用中文。API 名称、路径、导入语句、命令、代码示例和协议关键字保持原文；说明文字、表格列名、段落标题、备注和维护规则使用中文。稳定性不明确的公开符号标记为 `待复核`，不要继续使用英文 `Needs review`。

每个文档条目应包含：

- 类别
- 名称
- 类型
- 定义位置
- 导入 / include / use / 包路径
- 用途
- 最小用法示例
- 示例结果：标准输出、函数返回值、退出码、生成文件内容，或“无标准输出”的明确说明
- 必要的备注或限制

每个最小用法示例都必须同步给出可观察结果。Python 和其他 REPL 友好的脚本语言优先用交互式控制台形式展示返回值；C++、Go、Rust、.NET、Java 等语言可在代码块下方增加 `示例输出`、`示例返回值`、`示例结果` 或 `命令结果` 代码块，也可以在示例代码注释中标明返回值。若示例结果含随机值、时间戳、stacktrace、异步日志、Flink runtime 日志、Redis/网络环境差异等非确定内容，应使用占位格式或文字说明其形状，不要伪造固定输出。若示例本身不输出内容，也要说明返回值、断言结果、退出码、生成文件内容，或明确写出“无标准输出”。

文档应说明如何调用和使用能力，不暴露实现细节，也不要把内部源码复制到 README 中。私有辅助函数、内部函数、测试 fixture、生成文件、构建产物和第三方依赖代码不应记录，除非它们本来就是有意提供的可复用示例。

Python 和其他脚本语言工具在记录公共函数或类时，应包含 REPL / 交互式控制台用法。

示例：

```python
>>> from package.module import function_name
>>> function_name("input")
'output'
```

如果不确定某个符号是否应视为可复用公共 API，请先作为候选项记录，并标记为 `待复核`。

## Reference Routing

Read `references/repo-style.md` sections as needed:

- `Repository Shape` for workspace layout, devtool, CI, and skill update rules.
- `C++` for headers, ABI, feature macros, logging, concurrency, and CMake/test expectations.
- `Python` for package layout, DB/Redis/Mongo/MQ conventions, pytest patterns, and compatibility traps.
- `Go` for module layout, generic vs reflective APIs, labs, and benchmark artifacts.
- `Rust` for crate layout, trait/generic API style, features, examples, benches, and clippy/rustfmt.
- `.NET` for nullable C# library style, xUnit tests, packaging, and runtime roll-forward expectations.
- `Java` for Maven/Flink layout, Java 11 constraints, JUnit 5, JaCoCo, and Docker-run integration.
- `Devops, CI, Coverage` when editing workflows, Dockerfile, compose files, coverage extractors, Pages modules, or devtool commands.
- `references/agents-archive.md` for the full archived AGENTS content, including recent-work notes, detailed portability guidance, and the benchmark/performance protocol.

## Validation Shortlist

Use the narrowest matching command first:

```bash
./dev test-python
./dev test-go
./dev test-cpp
./dev test-rust
./dev test-dotnet
./dev test-java
```

For shared/public changes, add the matching `build-*`, `coverage-*`, `vet-*`, `fmt-*`, `guard-*`, `install-*`, and `./dev verify-install` commands when they apply.
