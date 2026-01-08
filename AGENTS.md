# AGENTS

This file captures recent work plus portability guidance, grouped by language.

## C++

### Recent Work (context)
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
