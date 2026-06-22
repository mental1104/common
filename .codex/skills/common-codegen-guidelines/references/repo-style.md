# Repo Style Reference

Use this reference after `SKILL.md` triggers. Load only the sections relevant to the requested language or workflow.

## Repository Shape

- Treat `/home/mental1104/code/common` as a multi-language common-library monorepo, not a single application.
- Main source roots are `cpp/`, `python/`, `golang/`, `rust/mental1104/`, `dotnet/`, `java/flink-datastream-demo/`, `export/`, `devops/`, `tools/ci/`, and `.github/workflows/`.
- Use `./dev` as the preferred entrypoint. It wraps setup, build, test, coverage, fmt, bench, install, uninstall, vet, guard, Docker, Java/Flink run, and verify-install commands.
- Keep CI and Pages conventions aligned: language workflows upload coverage artifacts, `ci-main.yml` coordinates main-branch execution, and coverage extraction scripts under `tools/ci/` produce normalized `cov.json`.
- `AGENTS.md` is a pointer-only compatibility file. Do not add new maintenance content there.
- When a change creates reusable rules or public workflow knowledge, update `.codex/skills/common-codegen-guidelines/SKILL.md` or the relevant file under `references/` in the same change.
- Preserve dirty worktree changes that are not yours. Read before editing files that already changed.

## C++

- Put public headers under `cpp/include/mental1104/...`; implementations live under `cpp/src/...`; examples belong under `cpp/examples/...`.
- Use namespace `mental1104` for public APIs. Keep public names consistent with nearby files.
- Use `cpp/include/mental1104/meta/compiler_support.h` feature macros: `M1104_CPLUSPLUS`, `M1104_HAS_CXX11/14/17/20/23`, `M1104_HAS_INCLUDE`, and `M1104_HAS_STRING_VIEW`.
- Prefer `mental1104::string_view` in public APIs. It maps to `std::string_view` on C++17+ and `std::string` on C++11/14.
- Do not introduce C++17+ features into shared headers unless guarded with repository macros and a lower-standard fallback.
- For C++11-compatible templates, use trailing return types with `decltype(...)` when return type deduction would require newer standards.
- Keep GCC/Clang/MSVC warning-clean. Avoid unused captures/parameters, narrowing conversions, signed/unsigned mistakes, and missing platform guards.
- For cross-platform code, isolate POSIX and Windows paths with `#ifdef _WIN32` or platform-specific source files. Do not assume Linux-only APIs in public headers.
- For C ABI or Python-exported surfaces under `export/cpp`, keep `extern "C"` APIs POD-only, stable, and exception-free. Return explicit status/result structs instead of throwing across ABI boundaries.
- Use existing concurrency helpers first: `IExecutor`, `ThreadPoolExecutor`, `BoostAsioExecutor`, coroutine schedulers, `MnCoroutinePool*`, and `detail::shared_mutex_t`.
- Use existing logging first: `mental1104/log.h`, `M1104_LOG_*`, and `M1104_LOG_*F`. Do not add another logging dependency.
- Preserve the file-local comment style. Chinese `用法` / `说明` comments are welcome when they explain non-obvious compatibility, ABI, concurrency, or algorithm decisions; avoid noisy comments.
- Public APIs need focused tests and, when useful, examples. If Redis++ or middleware behavior is touched, include the specialized devtool command or documented environment requirement.
- Typical validation: `./dev build-cpp`, `./dev test-cpp`, `./dev coverage-cpp`, `./dev fmt-cpp`, `./dev vet-cpp`, `./dev guard-cpp`, `./dev test-redispp`, `./dev install-cpp`, `./dev verify-install`.

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
- Typical validation: `./dev test-python`, `./dev coverage-python`, `./dev fmt-python`, `./dev vet-python`, `./dev guard-python`, `./dev install-python`, `./dev verify-install`.

## Go

- Module path is `github.com/mental1104/common/golang`; Go version is 1.22.
- Put library code under `golang/mental1104`, command-line tools under `golang/cmd/...`, internal helpers under `golang/internal/...`, labs under `golang/labs/...`, and docs under `golang/docs/...`.
- Keep public utility APIs simple and explicit. For convenience APIs, it is acceptable to use reflection with comments explaining semantics and performance tradeoffs; for hot paths, add generic typed alternatives.
- Use Chinese comments where they clarify API semantics, such as whether map containment means keys or values.
- Keep lab demos small, runnable, and documented. Use `internal/labkit` for shared lab run metadata instead of duplicating setup.
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

- Devtool command modules live under `devops/devtool/commands/...`; register commands through the existing `configure(subparsers)` and alias patterns.
- If adding a language-facing workflow, wire build/test/coverage/install/verify behavior through `./dev` first, then update CI to call the wrapper.
- Coverage extractors under `tools/ci/extract_coverage_*.py` should parse real reports into normalized `cov.json`; do not replace missing data with fake success.
- C++ CI uses `.github/actions/cpp-coverage-artifact` to keep `cov.json` extraction, `_cov` staging, and artifact upload behind one workflow step across Linux, macOS, and Windows.
- Pages generation should tolerate missing coverage by producing N/A badges/tables rather than broken links.
- Dockerfile changes should preserve layer caching: install stable tools and VSCode extensions before app code, then generate settings or build app artifacts after relevant source copies.
- Docker/proxy behavior should honor existing build args and env pass-through (`HTTP_PROXY`, `HTTPS_PROXY`, `NO_PROXY`, `ALL_PROXY`, custom feeds/mirrors).
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
