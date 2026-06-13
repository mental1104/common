---
name: common-codegen-guidelines
description: Repository-specific code generation and validation guide for /home/mental1104/code/common. Use when Codex creates or modifies code, tests, examples, devtool commands, CI/coverage plumbing, Docker/devops files, or multi-language public APIs in this repository, especially for C++, Python, Go, Rust, .NET, Java, cross-language ABI exports, benchmark/performance work, and AGENTS.md updates.
---

# Common Codegen Guidelines

Use this skill to make changes that look native to `/home/mental1104/code/common`.
This repository is a multi-language common-library workspace, so generated code must preserve each language's local conventions, devtool integration, and portability guarantees.

## Required Workflow

1. Read `AGENTS.md` before changing code. Treat it as the live repository contract.
2. Read the nearest existing implementation, tests, README, build file, and devtool command for the language you are touching.
3. For non-trivial code generation, read `references/repo-style.md`; for small single-language edits, read only the relevant language section.
4. Keep edits close to the existing module boundary. Prefer extending current helpers, aliases, registries, and command wrappers over adding a parallel style.
5. Add or update tests next to the existing tests for the touched language. Public APIs need behavior tests; cross-language/installation surfaces also need install or verify checks.
6. Validate with `./dev` commands whenever available. Prefer language aliases such as `./dev test-python`, `./dev coverage-cpp`, or `./dev verify-install` over direct tool invocations unless debugging a lower-level failure.
7. Update `AGENTS.md` whenever the change creates or changes repository-wide rules, public workflows, install/verify behavior, CI/coverage conventions, or reusable APIs that future agents must know.

## Generation Conditions

Generated code must satisfy these repository conditions:

- **Portability first**: support the repository's intended OS/compiler/runtime matrix; guard platform-specific code and avoid unguarded language-version features in shared surfaces.
- **Warning-clean**: keep C++/C#/Go/Rust/Java compiler and linter warnings clean because CI and some third-party integrations treat warnings as failures.
- **Local API consistency**: use `mental1104` namespaces/packages/modules and existing helpers for logging, concurrency, database scopes, message queues, coverage extraction, and devtool operations.
- **Validation evidence**: run the smallest meaningful build/test/coverage command, then broaden if the touched surface is shared.
- **No speculative performance claims**: for performance, benchmark, GC, scheduler, lock, contention, latency, or throughput work, follow the mandatory benchmark protocol in `AGENTS.md` before explaining conclusions.
- **No unnecessary churn**: do not reformat unrelated files, rewrite generated artifacts, or change CI/Docker layers unless required by the task.

## Reference Routing

Read `references/repo-style.md` sections as needed:

- `Repository Shape` for workspace layout, devtool, CI, and AGENTS update rules.
- `C++` for headers, ABI, feature macros, logging, concurrency, and CMake/test expectations.
- `Python` for package layout, DB/Redis/Mongo/MQ conventions, pytest patterns, and compatibility traps.
- `Go` for module layout, generic vs reflective APIs, labs, and benchmark artifacts.
- `Rust` for crate layout, trait/generic API style, features, examples, benches, and clippy/rustfmt.
- `.NET` for nullable C# library style, xUnit tests, packaging, and runtime roll-forward expectations.
- `Java` for Maven/Flink layout, Java 11 constraints, JUnit 5, JaCoCo, and Docker-run integration.
- `Devops, CI, Coverage` when editing workflows, Dockerfile, compose files, coverage extractors, Pages modules, or devtool commands.

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
