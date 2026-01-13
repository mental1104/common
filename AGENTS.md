# AGENTS

This file captures recent work plus portability guidance, grouped by language.

## General
- 有关于仓库的公共规范在每次更新完代码后都往AGENTS.md里及时更新。

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
