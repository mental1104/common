# Repo Style Reference

Use this reference after `SKILL.md` triggers. Load only the sections relevant to the requested language or workflow.

## Repository Shape

- Treat `/home/mental1104/code/common` as a multi-language common-library monorepo, not a single application.
- 目录组织、文件放置与新增目录约束，以及跨语言的最小完整用法测试首例规则，以根目录 `AGENTS.md` 为准；优先复用已有职责目录，不在此维护独立规则副本。
- Use `./dev` as the preferred entrypoint. It wraps setup, build, test, coverage, fmt, bench, install, uninstall, vet, guard, Docker, Java/Flink run, and verify-install commands.
- NAS 工作区的本地开发与验证统一在既有 `code-server` 容器挂载的仓库根目录运行 `./dev`，不要改在 NAS 宿主机或其他开发容器中执行。开发服务的镜像与部署配置在仓库外维护，不在本仓库恢复专用开发容器。其他平台与 CI 继续使用各自的原生工具链。
- 切换容器或仓库挂载路径时，Python 虚拟环境和 CMake 构建树不能直接视为可复用环境；先核对解释器、ABI 与缓存中的绝对路径，必要时备份旧目录后在当前环境重新生成。
- Keep CI and Pages conventions aligned: language workflows are reusable execution units, `ci-main.yml` routes `main` updates to the affected language workflows, and coverage extraction scripts under `tools/ci/` produce normalized `cov.json`.
- GitHub Actions 只由 `main` 分支 push 或仓库级手动 `Main Gate` 启动。`main` push 按提交前后差异选择受影响语言，并为选中的语言传入 `full_matrix: true`；手动 `Main Gate` 在 `main` 上展开六个语言的完整平台、编译器和运行时矩阵。PR、Draft PR 及普通功能分支不得分配 runner，也不得把 Actions 当作远程调试循环。
- Treat the complete `main`/manual GitHub Actions matrix as the repository compatibility contract. Local and branch-level validation should use `./dev`; it must not be used to raise the minimum compiler, language standard, runtime, or platform requirement.
- `AGENTS.md` holds directory organization, C++ declaration/implementation boundaries, and the minimum-complete-usage first-test rule at the user's explicit request; maintain those rules there. Other repository guidance remains in this skill.
- When a change creates reusable rules or public workflow knowledge, update `.codex/skills/common-codegen-guidelines/SKILL.md` or the relevant file under `references/` in the same change.
- 语言 README 中的可调用 API 文档默认用中文维护。API 名称、路径、导入、命令和代码示例保持原文，但说明文字、表格标签、维护规则、备注和复核标记使用中文；用 `待复核` 代替 `Needs review`。
- Preserve dirty worktree changes that are not yours. Read before editing files that already changed.

## C++

- C++ 文件放置及不再维护 examples 的规则见根目录 `AGENTS.md`。
- Follow the declaration/implementation rules in the repository-root `AGENTS.md`. Open templates remain visible to callers; ordinary runtime implementations belong in compiled sources, with optional dependencies and ABI configuration modeled explicitly.
- Use namespace `mental1104` for public APIs. Keep public names consistent with nearby files.
- Use `cpp/include/mental1104/meta/compiler_support.h` feature macros: `M1104_CPLUSPLUS`, `M1104_HAS_CXX11/14/17/20/23`, `M1104_HAS_INCLUDE`, and `M1104_HAS_STRING_VIEW`.
- C++ common-library code must honor the complete `.github/workflows/cpp.yml` matrix: Linux GCC/Clang, macOS Clang/GCC, Windows MSVC, and C++11, C++14, C++17, C++20, and C++23. Affected `main` updates and manual `Main Gate` runs exercise this complete matrix; treat C++11 as the baseline for shared headers and broadly reused source unless the target is explicitly gated out of older standards.
- Prefer `mental1104::string_view` in public APIs. It maps to `std::string_view` on C++17+ and `std::string` on C++11/14.
- JSON 按职责分成 `mental1104/json/codec.h` 与 `access.h`，调用方直接包含对应职责头，`codec.h` 包含 `access.h`，不再维护外层聚合头，对应实现为 `src/json/codec.cpp` 与 `access.cpp`；具体后端集中在私有 `src/json/detail/cjson.cpp`、`rapidjson.cpp`，每个文件完整包含原生 DOM、节点访问和编解码，外层不包含原生 SDK 头。`detail/backend.h` 只声明通用诊断、输入检查和数值转换，不暴露原生类型。公开头不按单个类型或模板实现拆成零碎文件；私有适配按后端归属维护。独立 `export/cpp` 必须递归收集 `src/json/`，避免遗漏 detail 实现。编解码职责集中 `load_json`/`dump_json` 和结果类型；对象访问职责集中逻辑类型、文档、后端节点读取和模板转换。`IJsonParser` 返回内部独占 `JsonDoc` 的 `ParseResult`，通过 `loaded.get()` 直接取得根视图，不公开 doc 成员；整个结果可移动、不可复制，布尔判断同时检查历史解析状态与当前所有权。应用后端通过 `ParseResult(JsonDoc(...))` 返回成功结果。`IJsonDocument` 派生类实现只读节点钩子，`JsonValueView` 借用文档并完成路径遍历和模板读取，严格读取统一为 `get_optional<T>()`（当前节点）及 `get_optional<T>(key/path)`，不保留 as_optional 同义入口；数组视图支持 range-for，非数组为空范围，游标与视图共同借用 DOM，索引访问复杂度取决于后端；禁止恢复包含原生类型的公共 variant 或裸节点注入接口。`JsonOptional` 为 C++17 标准 optional / 旧标准 Boost.Optional，`JsonValueReader<T>` 支持调用方特化。文本加载入口为 `load_json`，所有重载省略 parser 时固定使用 `JsonParser::CJSON`，禁用 cJSON 时返回明确错误，不自动切换后端；NUL 结尾字符串提供 const char* 入口以消除 string/string_view 重载歧义，nullptr 返回错误，有界输入继续使用指针/长度入口；不保留 `parse_json` 同义入口；`dump_json(const ParseResult&)` 直接接受加载结果，`dump_json(const JsonDoc&)` 支持独立拥有者，两者使用文档自身后端返回 `DumpResult`（ok/error/text），生成紧凑独立文本，不修改 DOM。后端文档在对应 detail 文件中覆写 `do_dump()`；默认钩子为只读后端返回不支持错误，禁止用后端枚举强制转换自定义文档。空文档和非有限数值不能静默输出 null；保留默认值读取，严格 optional 整数读取拒绝小数与越界；cJSON 的 double 精度、内嵌 NUL 和并发限制写在 `cpp/README.md`。`cmake/json.cmake` 独立选择后端，能力写入配置头，原生 SDK 为私有依赖；内置后端都关闭时仍可接入自定义实现。`test_json` 复用后端契约，`test_json_consumer` 不链接 GoogleTest，按库标准验证多翻译单元和自定义类型；安装消费时同时编译 `test/json/consumer.cc` 和 `consumer_main.cc`。
- Do not introduce C++17+ features into shared headers unless guarded with repository macros and a lower-standard fallback.
- When using C++14/17/20/23 features in C++, provide a C++11-compatible fallback, isolate the code behind feature macros/CMake conditions, or make the target unavailable only where the CI matrix already excludes it.
- For C++11-compatible templates, use trailing return types with `decltype(...)` when return type deduction would require newer standards.
- Keep GCC/Clang/MSVC warning-clean. Avoid unused captures/parameters, narrowing conversions, signed/unsigned mistakes, and missing platform guards.
- For cross-platform code, isolate POSIX and Windows paths with `#ifdef _WIN32` or platform-specific source files. Do not assume Linux-only APIs in public headers.
- For C ABI or Python-exported surfaces under `export/cpp`, keep `extern "C"` APIs POD-only, stable, and exception-free. Return explicit status/result structs instead of throwing across ABI boundaries.
- Use existing concurrency helpers first: `IExecutor`, `ThreadPoolExecutor`, `BoostAsioExecutor`, coroutine schedulers, `MnCoroutinePool*`, and `detail::shared_mutex_t`.
- Use existing logging first: `mental1104/log.h`, `M1104_LOG_*`, and `M1104_LOG_*F`. Do not add another logging dependency.
- HTTP/WebSocket 新模块独立于主库：公开类型在 `mental1104/web/server.h`，后端工厂在 `drogon_server.h`；应用不得接收 Drogon/Trantor 类型。构建定义放在 `cpp/cmake/web.cmake`，复用现有 `cpp/CMakeLists.txt` 入口启用 C++17+ 目标，安装包为 `mental1104Web`，目标为 `mental1104::web` / `mental1104::web_drogon`。主库仅在显式 `M1104_ENABLE_WEB=ON` 时引入，`src/web/` 必须从主库递归源码收集中排除。Drogon 为可选的私有共享库依赖，禁止提升原有 C++11/14 的基线或强制主库准备 Web SDK。后端契约测试为 `cpp/test/web/*.cc`；修改后端时复用这些行为测试，不再维护独立安装消费测试工程。第一版为进程内单次启动、非排空停止、字面路由和原始请求体，不提供 schema/multipart 编解码或 WebSocket 背压。
- Both the repository root and `cpp/` support ordinary CMake configure/build/CTest. At the user's explicit request, standalone native local configuration defaults `M1104_AUTO_SETUP=ON`: reuse devtool submodule/sparse-Boost preparation, build Redis SDKs under the current binary tree's `_deps`, and on Debian/Ubuntu prepare missing system packages with apt. CMake and a working C/C++ toolchain are startup prerequisites. Embedded builds, CI and cross compilation default automatic preparation off. Preserve `M1104_AUTO_SETUP`, `M1104_INSTALL_SYSTEM_DEPS`, and `AUTO_FETCH_SUBMODULES` opt-outs; do not overwrite populated source checkouts or share mutable SDK binaries across standard configurations. Use `PROJECT_SOURCE_DIR` for C++ source paths so the root wrapper works. See `cpp/cmake/bootstrap.cmake` and `devops/devtool/bootstrap_cpp.py`.
- Web `Server` 采用 NVI：基类固定配置、监听、发布就绪、事件循环与清理的阶段顺序；后端实现 protected `do_configure`、`do_listen`、`do_run_event_loop`、`do_cleanup`、`do_request_stop`、`do_is_io_thread`。监听钩子同步返回实际地址，公共接口不再暴露 Ready 回调。清理在成功或任一阶段异常时执行一次；析构必须先调用 `shutdown()`。Drogon 的合并式 app.run 通过内部 SDK 线程适配，监听阶段等真实就绪、事件循环阶段等 SDK 线程退出，清理阶段统一回收线程。阶段顺序、失败清理与取消语义由 `cpp/test/web/lifecycle_scenarios.cc` 验证。
- 编辑器的 `.clangd` 读取 `cpp/build/compile_commands.json`；VS Code CMake 配置启用 Web 目标。编辑可选后端前须确保该目录已使用 `M1104_ENABLE_WEB=ON` 配置，否则 clangd 会借用其他文件的命令，缺失 JsonCpp 等 SDK include 路径并产生模板签名恢复错误。命令行 Web 开关仍默认关闭；验证编辑器诊断时使用 clangd --check，不只检查 GCC 构建结果。
- Web 服务当前只管理一个监听器：使用 `ServerOptions::listener`，`start(Endpoint&)` 成功时输出单个实际地址，`do_listen()` 返回单个 `Endpoint`。默认 HTTP，已有 TLS 配置作用于同一个监听器；不要为假设中的多监听需求恢复 vector 接口。
- Web 注册在 Impl 中使用拥有路径副本的哈希索引：HTTP 键为方法与路径，WebSocket 键为路径；路由 vector 保留顺序且继续作为后端配置。重复检查、索引插入、列表保存均在公共状态锁内；列表保存失败必须回滚索引。索引仅在 Configuring 状态有效，启动冻结或启动前取消时整体释放，禁止使用借用 vector 元素的 string_view 作为索引键。
- Web 的公共 HTTP 规则在 `server.h` 声明、`server.cpp` 实现，内部适配辅助函数置于 `detail`：响应头规范化、状态码与下载名称校验、文件大小查询、单段 Range 筛选和解析。异常须由后端提交边界捕获。`drogon_server.cpp` 仅保留 SDK 类型转换、响应构造、回调及运行时适配；字面路径转正则属于 Drogon 路由 API 的适配，上传缓存目录用于 SDK 配置，两者保留。公共网络行为由 `server_scenarios.cc` 通过独立 Beast 客户端验证，不直接测试私有 helper。
- Web 后端场景通过 `m1104_add_web_backend_scenarios` 复用已编译的公共场景，每个实现只提供 `backend.h` 约定的工厂接入文件。CTest 每场景启动独立进程，筛选使用 `^(test_web_lifecycle|web\.)`；不要重新引入 installed 测试工程或旧 test_api/test_drogon 目标。GET 默认支持 HEAD，回调看到 Get，独立 Head 注册返回 false；不为拒绝隐式 HEAD 给 SDK 打补丁。Linux Drogon 的多 I/O 监听记录需校验地址一致，临时端口通过持续保留 SO_REUSEPORT 绑定统一分配。
- Drogon 的 BeginningAdvice 先于 startListening，不可直接据此发布启动成功。适配器通过 SDK 本地 TCP 探测确认端口可连接，五秒超时失败；不发送 HTTP/TLS 业务数据。Linux 的 StartWaitsUntilListenerAcceptsConnections 场景人为阻塞监听线程验证时序，客户端不应通过重试掩盖 start 提前返回的错误。
- Web 公共工厂、路由注册、`start(Endpoint&)`、`wait()` 返回 bool 并捕获内部异常，禁止恢复向业务层抛异常的接口。工厂通过空的 `unique_ptr<Server>&` 输出实例，失败保持输出原值；路由通过 const 引用传入并在捕获边界内复制。启动取消在 `complete_startup` 中与成功提交原子裁决：取消时不进入 Running 或事件循环，清理后 start/wait 返回 false。成功提交后的停止属于正常运行期停止。HTTP 转换异常映射 500，响应提交异常返回 false，会话发送异常返回 SendResult::Failed，SDK 回调不能让业务异常穿透事件线程。
- MQ SDKs are private imported targets in `cpp/cmake/mq.cmake`. Debian/Ubuntu automatic system preparation or explicit `M1104_INSTALL_MQ_DEPS=ON` installs distro `librdkafka-dev` first, then Pulsar repository packages when available or Apache 4.2.0 DEBs with official SHA512 checks. No extra apt source is required. Missing packages require root or noninteractive `sudo -n`; failures stop configuration. Unsupported hosts and cross builds provide SDK paths. Pulsar needs C++17+ and probes the linked SDK for `createProducerV2`; preserve the older API path. Document broker environment separately in `cpp/README.md`.
- Ordinary CTest includes functional tests; benchmark registration/build participation is opt-in via `M1104_REGISTER_BENCHMARK_TESTS=ON`. Keep `m1104_benchmarks` available to the existing `./dev bench-cpp` workflow when the option is off.
- In C++ implementation files, prefer `M1104_LOG_*` over direct `std::cout` / `std::cerr` for diagnostics unless the file is specifically testing stream behavior or intentionally writing a user-facing CLI stream.
- For C++ class implementations, qualify member field and helper-method access with `this->` when touching state such as `entries_` or `epoll_fd_`; constructor initializer lists and out-of-class function definitions remain unqualified by necessity.
- Preserve the file-local comment style. Chinese `用法` / `说明` comments are welcome when they explain non-obvious compatibility, ABI, concurrency, or algorithm decisions; avoid noisy comments.
- C/C++ 格式以仓库根 `.clang-format` 为准：四空格、不使用 Tab，续行四空格，lambda 函数体相对签名缩进一级，不按函数参数列大幅右移。编辑器关闭缩进自动探测，C/C++ 输入缩进也固定为四空格。修改时只格式化本次涉及的文件，避免批量改动无关历史代码。
- Public APIs need focused tests and, when useful, examples. If Redis++ or middleware behavior is touched, include the specialized devtool command or documented environment requirement.
- Typical validation: `./dev build-cpp`, `./dev test-cpp`, `./dev coverage-cpp`, `./dev fmt-cpp`, `./dev vet-cpp`, `./dev guard-cpp`, `./dev test-redispp`, `./dev install-cpp`, `./dev verify-install`. For shared C++ changes, locally exercise at least the oldest and newest relevant standards when feasible, for example `CXX_STD=11 ./dev build-cpp --config Debug` and `CXX_STD=23 ./dev build-cpp --config Debug`; rely on the post-merge/manual full CI for the complete OS/compiler/standard matrix.

## Python

- Put package code under `python/mental1104/...`; tests live under `python/test/...`; benchmarks live under `python/test_benchmark/...`.
- Follow nearby module style: type hints, explicit context managers, small helpers, and pytest tests with clear behavior names.
- Use existing DB APIs:
  `register_db`, `register_db_and_create`, `session_scope(DBKind.X)`, `tx_scope(DBKind.X)`, async variants, and aliases such as `pg_session_scope`, `mysql_session_scope`, `sqlite_session_scope`, and `ck_session_scope`.
- For DAOs, prefer `AutoSessionDAO` when methods should auto-inject `db`; public DAO methods should accept `*, db`. Pass `db=` explicitly when nesting scopes to avoid ContextVar overwrite.
- For Redis, use `register_redis`, `redis_params_from_env`, `redis_session_scope`, `ctx_redis_client`, and `RedisSessionAware`.
- For MongoDB, use sync/async nosql scopes and session-aware mixins. Transaction scopes require replica set or sharded support.
- For MQ tests, use `mental1104.mq.pulsar` and `mental1104.mq.kafka` helpers instead of duplicating connector setup.
- Keep compatibility deliberate. `python/pyproject.toml` declares a broad runtime floor, while current modules may use newer annotations. Match the touched file and CI target; in shared compatibility-sensitive modules, prefer `from __future__ import annotations` and `typing.Optional` / `typing.Union` where older Python parsing matters.
- Tests should avoid real services unless the existing test suite already gates them through env vars, fixtures, or Docker setup. Use `tmp_path` for filesystem behavior and skip at module import when optional dependencies cannot import on a runtime.
- Python system install must tolerate OS-managed packages without pip `RECORD` metadata. When installing requirements into system Python, detect such packages or retry with `--ignore-installed` instead of requiring users to remove Debian/Ubuntu packages by hand.
- Typical validation: `./dev test-python`, `./dev coverage-python`, `./dev fmt-python`, `./dev vet-python`, `./dev guard-python`, `./dev install-python`, `./dev verify-install`.

## Go

- Module path is `github.com/mental1104/common/golang`; Go version is 1.22.
- Put library code under `golang/mental1104`, command-line tools under `golang/cmd/...`, internal helpers under `golang/internal/...`, labs under `golang/labs/...`, and docs under `golang/docs/...`.
- Keep public utility APIs simple and explicit. For convenience APIs, it is acceptable to use reflection with comments explaining semantics and performance tradeoffs; for hot paths, add generic typed alternatives.
- Use Chinese comments where they clarify API semantics, such as whether map containment means keys or values.
- Keep lab demos small, runnable, and documented. Use `internal/labkit` for shared lab run metadata instead of duplicating setup.
- Go install/verify 的临时验证程序应使用 `go build -buildvcs=false`，避免本机 `/tmp/.git`、临时目录或 Git 状态异常影响幂等安装。
- For scheduler/GC/concurrency work, emit the mandatory Go artifacts from `references/agents-archive.md`: `metrics.csv`, `trace.out`, `mutex.pprof`, `block.pprof`, and `summary.txt`.
- Typical validation: `./dev build-go`, `./dev test-go`, `./dev coverage-go`, `./dev fmt-go`, `./dev vet-go`, `./dev guard-go`, `./dev install-go`, `./dev verify-install`.

## Rust

- Crate root is `rust/mental1104`; edition is 2021; `rust-toolchain.toml` requests stable with `clippy` and `rustfmt`.
- Public modules live under `src/`; expose common imports through `prelude`; put integration tests under `tests/`, examples under `examples/`, and Criterion benches under `benches/`.
- Prefer trait + generic designs for unified APIs instead of runtime type switching. Document why the abstraction is zero-cost when that is the design goal.
- Use feature flags for optional dependencies and keep performance-oriented features disabled by default unless the task explicitly changes defaults.
- Keep `#[inline]` focused on tiny generic/public hot-path functions. Do not sprinkle it over non-hot code.
- Include module tests for core behavior and integration tests for public re-export behavior.
- Typical validation: `./dev build-rust`, `./dev test-rust`, `./dev coverage-rust`, `./dev fmt-rust`, `./dev vet-rust`, `./dev guard-rust`, `./dev clippy-rust`, `./dev example-rust`, `./dev install-rust`, `./dev verify-install`.

## .NET

- Source lives under `dotnet/src/Mental1104`; tests live under `dotnet/tests/Mental1104.Tests`.
- Target `net8.0`; `Nullable` is enabled. Keep null behavior explicit and avoid nullable warnings.
- Use file-scoped namespaces and xUnit tests, matching nearby style.
- For binary/interop code, prefer deterministic failure behavior. Public validators should return stable results for malformed input rather than leaking low-level exceptions, unless the API contract says otherwise.
- Build test fixtures in-memory where practical, as `ExeCheckerTests` does for PE headers.
- Devtool may roll forward to newer runtimes when `net8` is missing, but project files should keep target frameworks stable unless changing the repo policy.
- Typical validation: `./dev build-dotnet`, `./dev test-dotnet`, `./dev coverage-dotnet`, `./dev fmt-dotnet`, `./dev vet-dotnet`, `./dev guard-dotnet`, `./dev install-dotnet`, `./dev verify-install`.

## Java

- Current Java project is `java/flink-datastream-demo`, a single-module Maven project.
- Use package roots `com.mental1104.common` for reusable utilities and `com.mental1104.flink.impl/examples/tests` for the Flink demo.
- Target Java 11 unless the Maven property changes. Avoid APIs introduced after Java 11.
- Use `final` utility classes with private constructors for static helper APIs.
- Use JUnit 5 tests under `src/test/java`; keep tests direct and behavior-focused.
- Keep Flink job logic in `impl` so examples stay thin entrypoints and tests can exercise pipelines without launching a cluster.
- Preserve `.mvn/jvm.config` module-open workaround for JDK 17+ Flink exec runs.
- Typical validation: `./dev setup-java`, `./dev build-java`, `./dev test-java`, `./dev coverage-java`, `./dev run-java`, `./dev run-java-docker`, `./dev install-java`, `./dev verify-install`.

## Devops, CI, Coverage

`./dev setup-cpp` 在准备子模块前检查宿主机编译依赖；调整 C++ 的必需系统依赖或覆盖率工具时，同步更新该预检及 `cpp/README.md`。

- Devtool command modules live under `devops/devtool/commands/...`; register commands through the existing `configure(subparsers)` and alias patterns.
- If adding a language-facing workflow, wire build/test/coverage/install/verify behavior through `./dev` first, then update CI to call the wrapper.
- Each language workflow exposes `full_matrix` through `workflow_call`. Keep one shared set of steps: callers use `full_matrix: true` for affected `main` updates and repository-level manual validation. Do not create parallel `quick-*` and `full-*` workflow copies.
- `ci-main.yml` is the only GitHub Actions entrypoint. A `main` push computes changed files from `github.event.before` to `github.sha`, calls only affected language workflows with `full_matrix: true`, and publishes Pages only when walkthrough/CI presentation inputs changed. A manual run is restricted to `main`, calls all language workflows with `full_matrix: true`, does not deploy Pages, and sends mail only when that manual gate fails. Pull requests never start Actions.
- If a workflow matrix, local action, or required compiler/runtime changes, update this skill reference so future code generation follows the complete compatibility contract and local `./dev` validation boundary.
- Coverage extractors under `tools/ci/extract_coverage_*.py` should parse real reports into normalized `cov.json`; do not replace missing data with fake success.
- C++ CI uses `.github/actions/cpp-coverage-artifact` to keep `cov.json` extraction, `_cov` staging, and artifact upload behind one workflow step across Linux, macOS, and Windows.
- Pages generation should tolerate missing coverage by producing N/A badges/tables rather than broken links.
- Dockerfile changes should preserve layer caching: install stable tools and VSCode extensions before app code, then generate settings or build app artifacts after relevant source copies.
- Docker/proxy behavior should honor existing build args and env pass-through (`HTTP_PROXY`, `HTTPS_PROXY`, `NO_PROXY`, `ALL_PROXY`, custom feeds/mirrors`).
- Compose changes should preserve the offline-network intent and published localhost ports unless the task is specifically about networking.
- Typical validation: `./dev run-docker`, `./dev build-docker` when Docker changes are central, plus the touched language's build/test/coverage commands.

## Benchmark And Performance Protocol

For any performance, benchmark, latency, throughput, scheduler, GC, concurrency, lock, contention, or data-backed claim, follow the archived protocol in `references/agents-archive.md`:

1. Define the unit of work and fix inputs, concurrency, duration, and iteration counts.
2. Use same-binary A/B: implement both variants in one program and run them sequentially in the same process when possible.
3. Warm up and run at least five trials.
4. Collect artifacts before writing conclusions.
5. Explain likely bias sources: sampling overhead, scheduling jitter, GC cycle timing, lock contention, cache effects, and system noise.
6. Provide one reproducible command with build and run parameters.
7. Tie every claim to metrics. For example, use CPU user/sys for compute vs syscall overhead, p99/max plus context switches for tail latency, and Go mutex/GC metrics for contention or allocation pressure.
