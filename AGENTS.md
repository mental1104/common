# AGENTS

This file captures recent work plus portability guidance, grouped by language.

## General
- 有关于仓库的公共规范在每次更新完代码后都往AGENTS.md里及时更新。
- Devops: `devops/INSTALLROOT/root/extensions.json` stores `common` defaults and per-extension settings placeholders for VSCode server settings assembly.
- Devops: `devops/INSTALLROOT/root/extensions.list` drives VSCode extension installation independently from settings changes.
- Devops: `devops/INSTALLROOT/root/vscode_extensions.py` accepts JSONC and preserves comments when rendering settings.json.
- Dockerfile: VSCode extension install runs before app code; settings generation runs after app code to keep installs cached.
- Devops: VSCode settings generation is maintained in `devops/INSTALLROOT/root/vscode_extensions.py` and invoked by Dockerfile.
- Dockerfile: extension install now summarizes failed extension ids and skips blank/commented lines in the list.
- Dockerfile: extension install now uses the VSCode server `code-server` script (with fallback search) and fails the build on install errors.
- Dockerfile/devtool: add `NUGET_SOURCE` build arg passthrough to allow restoring .NET packages from a custom feed or mirror.
- Pages: build/deploy is triggered only by the main-branch meta workflow via workflow_call; coverage artifacts come from the same run (no workflow_run/run-id lookup).
- Pages: deploy retries handle in-progress Pages deployments without relying on the Pages API.
- Coverage artifacts: stash to `_cov/<lang>/<key>/cov.json` and upload as `<lang>-cov-*`; pages always generates per-language modules with N/A badges to avoid 404s.
- Coverage modules: Rust/.NET/Go now emit per-OS, per-version badges and tables aligned to current CI matrices.
- README: Rust/.NET/Go coverage sections show per-OS, per-version badge matrices.
- CI: language workflows ignore push-to-main; main branch runs only via `ci-main.yml` to prevent duplicate executions.
- CI: Python workflow now fails immediately on setup/build/coverage errors (removed deferred failure marker).
- CI: Linux python matrix job now starts middleware services and exports env vars for 3.8–3.14 runs.
- CI: Linux python jobs align ClickHouse auth and switch MySQL test user to mysql_native_password for CI compatibility.
- CI: Removed redundant linux-integration job since the Linux matrix covers middleware tests.
- CI: C++ linux matrix now starts Redis service and exports REDIS_* envs so redis_lock tests run per C++ standard.
- CI: Removed cpp linux-integration job; redis tests run in the main C++ matrix.
- CI: Restored Linux g++ C++20 in the C++ matrix.
- CI: C++ clang coverage installs matching llvm-<ver>-tools and sets GCOV to llvm-cov-<ver> gcov.
- Tests: Redis multiprocessing tests now use module-level workers and per-process connections to support spawn/forkserver (Python 3.14).
- 测试: Redis 多进程 helper 注释说明 spawn 序列化与子进程连接策略。
- Coverage extractors: add Rust/Go/.NET cov.json extract scripts and wire workflows to parse real reports instead of placeholders.
- DataStructure: add per-OS/C++-standard coverage artifacts, pages badges/dashboard, and README coverage matrix.
- DataStructure: make coverage emits coverage.xml for artifact extraction and ignore coverage artifacts via subrepo .gitignore.
- Dev tool: add dotnet setup/build/test/coverage/fmt/bench/clean/install/uninstall/vet/guard commands; dotnet workflow now runs via `./dev` (linux/mac) or `python -m devtool.cli` (windows).
- Dev tool: add build-docker/push-docker commands (Linux-only) with SSH_PRIVATE_KEY env requirement and docker.io login via DOCKER_USERNAME/DOCKER_PASSWORD.
- Dev tool: go build now emits main-package binaries (if any) and logs when only library packages are compiled.
- Dev tool: dotnet test/coverage now auto-roll forward to newer runtimes if net8 is missing (warns), while keeping net8 target frameworks.
- Rust coverage: dev coverage resolves LLVM_COV/LLVM_PROFDATA via rustc sysroot (lib/rustlib/<host>/bin) plus rustup/xcrun/brew when llvm-tools-preview is unavailable.
- Rust coverage: rustup lookup noise is suppressed; sysroot tools are preferred to avoid spurious errors.
- Rust coverage: fallback now probes rustup toolchain sysroot (`RUSTUP_TOOLCHAIN` or stable) when PATH rustc lacks llvm tools.
- Rust coverage: set `RUST_COVER_XML=1` (optional `RUST_COVER_XML_PATH`) to make `./dev coverage-rust` emit Cobertura XML for local cov.json extraction.
- Rust coverage: Cobertura XML is generated under `rust/mental1104/coverage.xml` to match the crate root.
- Dockerfile: align INSTALLROOT inputs to current files; fetch Go/VSCode server/okteto/syncthing at build time; use `./dev build/install` for C++/Python installs.
- Dockerfile: run `DOTNET_CLEAN_ALLOW_FAIL=1 ./dev clean-all`, skip boost submodule, and `./dev setup-dotnet` before dotnet build; install Go/Rust/.NET artifacts for reuse with Go workspace, Cargo patch config, and local NuGet feed.
- Dev tool: dotnet clean now falls back to manual bin/obj cleanup when `DOTNET_CLEAN_ALLOW_FAIL=1` to avoid missing package errors.
- Dev tool: dotnet clean no longer passes unsupported `--no-restore` to avoid MSBuild switch errors.
- Dev tool: C++ install now skips empty `SUDO` entries to avoid Docker PermissionError.
- Dev tool: Rust setup now skips rustup toolchain override when rustup is missing (warns and continues with system toolchain).
- Dockerfile: install rustup/cargo to `/usr/local` and add `/usr/local/cargo/bin` to `PATH` for rust builds.
- Dev tool: add `setup-dotnet` command to run dotnet restore via `./dev`.
- Dev tool: Python install now retries pip/setuptools/wheel upgrade with `--ignore-installed` when Debian-installed wheels lack RECORD.
- Dev tool: Python install now installs dependencies from requirements by default; set `PY_INSTALL_NO_DEPS=1` to skip, and wheel installs stay `--no-deps` to avoid file URL metadata errors.
- Dev tool: add `verify-install`/`install-verify` command to smoke-test C++/Python/Go/Rust/.NET install outputs from a temp project.
- Dev tool: `verify-install` now resolves dotnet path via repo root instead of missing common constant.
- Dev tool: `verify-install` registers `install-verify` via argparse aliases to avoid duplicate subparser conflicts.
- Dev tool: `verify-install` Go check uses a local module replace to avoid network module lookups.
- Dev tool: `verify-install` Go check now imports `GO_DIR` for the local replace.
- Dev tool: `verify-install` Go check forces offline env (GOWORK/GOPROXY/GOSUMDB) to avoid proxy lookups.
- Dev tool: `verify-install` Rust check uses POSIX-style path on Windows to avoid TOML escape errors.
- Dev tool: export C++ build now drops stale CMakeCache from another path before configuring.
- Dev tool: `clean-python` now also removes `export/cpp/build` to keep Python export artifacts in sync.
- Dev tool: add `run-docker` to restart root compose stack with `docker compose` preferred (fallback to docker-compose) and idempotent down.
- Dev tool: `run-docker` now runs `docker compose up -d --build --force-recreate`.
- Dev tool: `verify-install` runs Rust check in offline mode to avoid crates.io lookups in isolated containers.
- Dev tool: `verify-install` now prints cpp/rust/dotnet success markers.
- Dev tool: `verify-install` C++ check on Windows recognizes MSVC lib naming and uses CMake plus PATH for runtime.
- CI: add per-language `verify-install` steps at the end of matrix jobs (cpp/python install before verify).
- Dev tool: C++ install now passes `--config` for multi-config build dirs.
- Dev tool: C++ verify skips explicit include path when PREFIX is `/usr/local` to validate system default include search.
- Compose: bind SSH to localhost 31104 and force sshd as the container command while keeping internal network.
- Compose: switch offline network to bridge with IP masquerade disabled to allow port publishing while blocking egress.
- Dockerfile: drop the `# syntax=docker/dockerfile:1.7` header to avoid network pulls for the frontend image.
- Dockerfile/devtool: allow overriding the base image via `BASE_IMAGE` build arg (`DOCKER_BUILD_BASE_IMAGE`/`DOCKER_BASE_IMAGE`).
- Dockerfile: install pip deps from `python/requirements.txt` and strip the local `mental1104_export_layer` line for image builds.
- Dockerfile: set rustup mirrors via `RUSTUP_DIST_SERVER`/`RUSTUP_UPDATE_ROOT` to avoid rust-lang TLS failures.
- Dockerfile: split language build/install into separate layers (dotnet, rust, go, cpp, python) to improve caching.
- Dockerfile: install VSCode server/cli offline using `ARG VSCODE_COMMIT` + `TARGETARCH` into `/root/.vscode-server/cli/servers/Stable-*` and `code-*` paths (documented via `code --version`).
- Dockerfile: fix VSCode server commit hash typo (remove leading 9) to avoid 400 download errors.
- Dockerfile: replace apt sources heredoc with printf to avoid BuildKit heredoc parsing errors.
- Dockerfile: install ca-certificates before adding clickhouse repo; switch to libncursesw6 and make MongoDB client install optional with mongosh fallback.
- Dockerfile: add ClickHouse key URL fallback (packages.clickhouse.com -> mirror) and install clickhouse-client only if repo setup succeeds.
- Dockerfile: disable proxy env for all apt-get update/install runs while leaving curl to use proxies.
- Dev tool: build-docker auto-uses host network for localhost proxies and auto-adds host.docker.internal mapping when needed.
- Dockerfile: add curl retries and Okteto download fallbacks to GitHub release assets.
- Dockerfile: avoid GitHub API for syncthing latest; resolve tag via releases redirect/HTML.
- Dockerfile/devtool: accept proxy build args and forward HTTP(S)_PROXY/NO_PROXY from `./dev build-docker`.
- Dockerfile/devtool: also forward ALL_PROXY and allow passing `--network`/`--add-host` via env for local proxy access.

### Performance: Non-IO Benchmark Protocol (must follow)

#### Trigger
- 任何涉及：performance/benchmark/latency/throughput/scheduler/GC/concurrency/lock/contention 的任务
- 任何要求“用数据佐证结论”的题（尤其是调度/GC/并发）

#### Mandatory workflow (do not skip)
1) Define unit-of-work（一次迭代代表什么），并固定：输入数据、并发度/线程数、运行时长/迭代次数
2) Same-binary A/B：同一程序内实现两版本（A 与 B），同一进程内顺序跑，避免环境漂移
3) Warmup + Trials：预热后至少 5 次 trial；输出每次 trial 的摘要，并计算 CV%
4) Collect evidence first：先产出数据/剖析文件，再写解释与结论（禁止“拍脑袋结论”）
5) Explain bias：必须解释偏差来源（采样开销/调度抖动/GC 周期/锁竞争/缓存抖动）
6) Repro commands：给出一条命令可在干净环境复现（含编译与运行参数）

#### Mandatory outputs (artifacts)
- C++：stdout 里必须包含
  - latency: p50/p95/p99/max（单位明确）
  - throughput: ops/s
  - CPU: user/sys + cpu%
  - memory: RSS（峰值或采样口径明确）
  - ctx switch: voluntary/involuntary
  - stability: mean/std/CV%（至少对 ops/s 与 p99）
- Go（调度/GC/并发题必须额外给曲线/剖析）：
  - metrics.csv（时间序列曲线：cpu%、goroutines、heap_inuse、gc_cycles、gc_pause、mutex_wait）
  - trace.out（go tool trace）
  - mutex.pprof、block.pprof（go tool pprof）
  - summary.txt（每个 trial 的 wall、ops/s、cpu%、user/sys）

#### Metric -> Claim contract (must be self-consistent)
- cpu%≈100 且 sys≈0：compute-bound（纯计算/算法差异）证据：cpu% + user/sys
- sys 占比上升：syscall/内核态开销上升 证据：sys(s) 或 profile/trace 里 syscall
- p99/max 拉高且 ctx switch 上升：调度噪声/抢占/锁竞争放大尾延迟 证据：p99/max + ctxsw
- Go：mutex_wait_total_s 或 count 上升：锁竞争恶化 证据：metrics.csv + mutex.pprof 栈
- Go：gc_cycles/pauses 上升且 heap_inuse 更高：分配压力/GC 频率增加 证据：metrics.csv + trace.out GC 密度

#### Acceptance (Definition of Done)
- 能清楚说出“哪个指标证明哪件事”，并且与产物（stdout/csv/pprof/trace）一一对应、自洽
- 多次 trial 结论一致（至少关键指标差异方向一致；并说明 CV% 与噪声来源）


## C++

### Recent Work (context)
- Bloom filter test: precompute per-thread slice bounds to avoid unused lambda captures under clang warnings-as-errors.
- C++11/14 compatibility fixes: JSON now uses `mental1104::string_view` and a variant wrapper (std::variant in C++17+, boost::variant2 fallback). Avoided C++17-only constructs in headers.
- Redis++ ABI alignment: `M1104_REDISPP_CXX_STANDARD` is injected from CMake (`cpp/cmake/deps.cmake`) and used in `redis_lock.h` to select the right redis++ namespace/ABI helpers.
- C++11 fix in bloom filter: function templates use trailing return types instead of C++14 auto-deduction.
- CI now runs coverage steps instead of separate test steps in GitHub Actions (tests are included in coverage runs). `test-redispp` remains a standalone test step.
- Boost sparse checkout includes `mp11` and `variant2` to support the C++11/14 JSON fallback.
- Stacktrace: add C/C++ crash capture with JSON Lines output, POSIX forked symbolization, Windows DbgHelp path, and C/C++ examples.
- Stacktrace: fix macOS ucontext build by defining `_XOPEN_SOURCE`, adjust POSIX symbolizer helper signatures to avoid const-pointer warnings.
- Stacktrace: use `pthread_self` for macOS thread id and avoid deprecated `getcontext` on macOS manual dumps.
- Stacktrace: add macOS arm64 register extraction for IP/SP/BP and improve atos output parsing for file:line.
- Stacktrace: move public header under `cpp/include/mental1104/debug` and update include paths/docs.
- Gitignore: unignore `cpp/include/mental1104/debug` so stacktrace headers are tracked.
- Stacktrace: add pluggable formatter (JSON/Python-like), new formatting options, and route stack output through formatter hooks.
- Stacktrace: undef MSVC `exception_code` macro in Windows implementation to avoid struct field name collision.
- Examples: deepen C function call chain and add class-method stack depth for C++ crash demo.
- Examples: move stacktrace demos into `cpp/examples/debug/stacktrace` with a Chinese README guide.
- Docs: convert `cpp/README.md` to Chinese and update stacktrace usage details.
- Dockerfile: add webbench install step with `WEBBENCH_VERSION` build arg and install `exuberant-ctags`.
- Dockerfile: add a dedicated layer for toy commands (sysvbanner/toilet/figlet/cowsay/aafire).

### Concurrency & Logging APIs (prefer these)

#### Concurrency (use `cpp/include/mental1104/concurrency/*` first)
- `concurrency/executor.h`: `mental1104::IExecutor::execute` fire-and-forget task submission.
- `concurrency/coroutine/task.h`: `mental1104::Task` coroutine handle wrapper (`resume`, `done`, `native_handle`).
- `concurrency/coroutine/coroutine_scheduler.h`: `mental1104::ICoroutineScheduler::spawn_task` / `wait_all`; `BasicCoroutineScheduler` runs tasks on an `IExecutor` with a ready queue.
- `concurrency/coroutine/async_simple_scheduler.h` (C++20 + async_simple): `AsyncSimpleCoroutineScheduler` schedules `Task` via `async_simple::Executor`.
- `concurrency/mn/mn_coroutine_pool.h`: `MnCoroutinePoolT::spawn` / `wait_all` plus accessors; aliases `MnCoroutinePool`, `MnCoroutinePoolAsyncSimple`, `BoostMnCoroutinePool`, `BoostMnCoroutinePoolAsyncSimple`.
- `concurrency/thread/thread_util.h`: `mental1104::sleep_for` / `sleep_for_ms`; `ThreadPool::submit` returns `std::future` (note: `ThreadPool` is in global namespace).
- `concurrency/thread/thread_pool_executor.h`: `ThreadPoolExecutor` implements `IExecutor` (and `async_simple::Executor` when available); `underlying_pool()`.
- `concurrency/thread/boost_asio_executor.h`: `BoostAsioExecutor` implements `IExecutor` (and `async_simple::Executor` when available); `underlying_pool()`.
- `concurrency/lock/shared_mutex.h`: `mental1104::detail::shared_mutex_t` + `shared_lock_t` alias to the newest available shared mutex type.

#### Logging (use `cpp/include/mental1104/log*` first)
- `log.h`: `mental1104::LogLevel` enum; `get_log_level` / `set_log_level`; `log` and `logf` helpers.
- `log.h` macros: `M1104_LOG_DEBUG/INFO/WARNING/ERROR` and formatted `M1104_LOG_*F`.
- `log.h` behavior: uses spdlog when available, otherwise stdout/stderr; log level from `MENTAL1104_LOG_LEVEL` or `M1104_LOG_DEFAULT_LEVEL`.
- `log/adapters/cache_printer.h`: `log_detail::format_value` for `LRUCache`/`LFUCache` structured logging.

### Portability Guidance

#### Cross-standard (C++11/14/17/20/23)
- Use feature macros from `cpp/include/mental1104/meta/compiler_support.h`:
  `M1104_HAS_CXX11/14/17/20/23`, `M1104_HAS_INCLUDE`, `M1104_HAS_STRING_VIEW`.
- Prefer `mental1104::string_view` in public APIs; it aliases to `std::string_view` on C++17+ and to `std::string` on C++11/14.
- Avoid C++17-only features (e.g., `std::variant`, `std::optional`, inline variables, `if constexpr`) unless guarded with the macros above.
- For template return types in C++11, use trailing return types with `decltype(...)`.

#### Cross-compiler (GCC/Clang/MSVC)
- MSVC: rely on `M1104_CPLUSPLUS` (uses `_MSVC_LANG`) instead of raw `__cplusplus`.
- GCC/Clang: keep warnings clean under `-Wall -Wextra` to avoid CI failures.
- Avoid compiler-specific extensions unless behind a compile-time check.

#### Cross-platform (Linux/macOS/Windows)
- macOS: Homebrew LLVM clang should use libc++ (include and lib paths), avoid mixing libc++/libstdc++.
- Windows: dynamic CRT is enforced in CMake (`CMAKE_MSVC_RUNTIME_LIBRARY`); avoid POSIX-only APIs or guard with `#ifdef _WIN32`.
- Linux: build under both GCC and Clang; avoid UB and rely on feature macros instead of compiler detection.

#### Cross-language / ABI
- C++ is consumed by Python via `export/cpp` (C API + pybind). Keep `extern "C"` APIs stable and POD-only.
- Do not throw exceptions across C ABI boundaries; return errors via result structs/strings.
- Avoid exposing STL types in C APIs; keep ownership/allocator rules explicit.

#### Warnings-as-Errors Culture
- Several third-party components and CI configurations treat warnings as errors (`-Werror` or `/WX`), so new warnings can fail builds.
- Code should be warning-clean across GCC/Clang/MSVC (unused params, narrowing, sign conversions, missing initializers, etc.).

## Python

### Recent Work (context)
- ClickHouse tests: skip at module import on Python < 3.9 to avoid clickhouse_connect typing errors.
- Python setup: install poetry-core alongside pip/setuptools/wheel to support PEP 517 builds with --no-build-isolation (macOS asyncmy).
- Python setup: install Cython to build asyncmy from source on Python 3.13.
- Python deps: add greenlet to satisfy SQLAlchemy asyncio on macOS.
- Coverage: omit `python/test` from Python coverage runs.
- DB registry now keys by `(DBKind, db_name)`; `db_name` defaults to `default`.
- Scopes now take kind first: `session_scope(DBKind.X, db_name="default")` and async variants.
- Added scope aliases: `pg_session_scope`, `mysql_session_scope`, `sqlite_session_scope`, `ck_session_scope` and matching `*_tx_scope`.
- Added `AutoSessionDAO` (auto-injects `db` keyword param) and optional singleton support.
- Added `register_db_and_create` / `register_db_and_create_async` (register + optional create_all).
- Utils: add batch_rename helpers for planning/applying renames with suffix/regex/index rules plus tests.
- Utils: add Chinese usage comments for batch_rename functions.
- Utils: use typing.Union for batch_rename type aliases to keep Python 3.8/3.9 compatible.
- Pulsar: move AbstractMessageQueue into connector/mq module and keep PulsarMessageQueue using it.
- Dev tool: Python coverage now installs requirements if coverage isn't in the venv (keeps CI from failing on missing coverage).
- Dev tool: uninstall now mirrors install (supports `--prefix`), and verify-install can require installed artifacts via `VERIFY_REQUIRE_INSTALL=1`.
- Dev tool: Go/Rust install now emits verify binaries and dotnet install packs into the local feed so uninstall/verify-uninstall can validate removal.
- CI: Windows verify-uninstall steps now use continue-on-error plus an assert step to enforce expected failure.
- Dev tool: C++ uninstall now falls back to sudo rm when install files are root-owned (e.g., sudo installs in CI).
- Dev tool: Windows system pip upgrades now use `python -m pip` to avoid pip self-update failures in CI.

### DB Usage (latest)
- Register once per process: `register_db(DBKind.POSTGRES, dsn=..., db_name="default")`.
- Optional create: `register_db_and_create(DBKind.POSTGRES, dsn=..., create=True)`.
- Read: `session_scope(DBKind.X)`; write/mixed: `tx_scope(DBKind.X)`.
- Async: `async_session_scope(DBKind.X)` / `async_tx_scope(DBKind.X)`.
- DAO pattern: `AutoSessionDAO` methods should be `def create(self, ..., *, db)` and called inside scope without passing `db`.
- Multi-DB flow: open separate scopes per DB; pass `db=` explicitly when nesting to avoid ContextVar overwrite.

### Connection Methods (common DSNs)
- PostgreSQL: `postgresql+psycopg://user:pass@host:port/db`.
- MySQL: `mysql+pymysql://user:pass@host:port/db`.
- SQLite (params): `ConnParams(ip="path.sqlite3")` or `ConnParams(ip=":memory:")`.
- SQLite (dsn): `sqlite+pysqlite:///path.sqlite3`.
- ClickHouse (SQLAlchemy dialect): use dialect-specific DSN (if installed).
- ClickHouse (clickhouse-connect): `register_db(DBKind.CLICKHOUSE, dsn="clickhouse://...", options={"driver": "connect"})`.
  ClickHouse `tx_scope` is a no-op wrapper; no ACID transactions.
- ClickHouse distributed: `profile=ClickHouseProfile.DISTRIBUTED` with `options={"cluster": "cluster_name"}`; `create_all/drop_all` auto inject `ON CLUSTER`.

### Redis (db/redis)
- Register once: `register_redis(params=redis_params_from_env())`; use `redis_session_scope()` for read/write.
- Cluster: `mode=RedisMode.CLUSTER` + `options={"startup_nodes": "host1:6379,host2:6379"}` or env `REDIS_MODE=cluster` + `REDIS_CLUSTER_NODES`.
- Sentinel: `mode=RedisMode.SENTINEL` + `options={"sentinels": "...", "service_name": "mymaster"}` or env `REDIS_MODE=sentinel` + `REDIS_SENTINELS` + `REDIS_SENTINEL_SERVICE`.
- ContextVar helpers: `ctx_redis_client()` / `require_ctx_redis_client()`; `RedisSessionAware` mixin provides `_redis()`.

### MongoDB (db/nosql, sync)
- Register once: `register_mongo(params=mongo_params_from_env())`; use `mongo_session_scope()` for read/write.
- ContextVar helpers: `ctx_mongo_session()` / `require_ctx_mongo_session()`; `MongoSessionAware` provides `_mongo()`.
- `mongo_tx_scope()` starts a session + transaction; requires replica set or sharded cluster support.
- `AutoMongoSessionDAO` can auto-inject a `mongo` parameter from ContextVar; DAO methods should accept `*, mongo`.

### MongoDB (db/nosql, async)
- Use `async_mongo_session_scope()` / `async_mongo_tx_scope()`; `AsyncMongoSessionAware` provides `_amongo()`.
- Requires `motor` dependency; same env vars as sync.
- `AutoMongoSessionDAO` also wraps async DAO methods and injects `mongo`.
