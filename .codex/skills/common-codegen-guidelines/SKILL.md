---
name: common-codegen-guidelines
description: Repository-specific code generation and validation guide for /home/mental1104/code/common. Use when Codex creates or modifies code, tests, examples, devtool commands, CI/coverage plumbing, Docker/devops files, or multi-language public APIs in this repository, especially for C++, Python, Go, Rust, .NET, Java, cross-language ABI exports, benchmark/performance work, and updates to this repo skill's guidance.
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

- **CI matrix is the coding contract**: write code to satisfy the current GitHub Actions platform, compiler, runtime, language-standard, build, test, install, and coverage matrix defined under `.github/workflows/` and `.github/actions/`.
- **Portability first**: support the repository's intended OS/compiler/runtime matrix; for reusable common-library code, treat the lowest CI-supported language/runtime version as the baseline and guard newer language-version features in shared surfaces.
- **Warning-clean**: keep C++/C#/Go/Rust/Java compiler and linter warnings clean because CI and some third-party integrations treat warnings as failures.
- **Local API consistency**: use `mental1104` namespaces/packages/modules and existing helpers for logging, concurrency, database scopes, message queues, coverage extraction, and devtool operations.
- **Validation evidence**: run the smallest meaningful build/test/coverage command, then broaden if the touched surface is shared.
- **No speculative performance claims**: for performance, benchmark, GC, scheduler, lock, contention, latency, or throughput work, follow the mandatory benchmark protocol in `references/agents-archive.md` before explaining conclusions.
- **No unnecessary churn**: do not reformat unrelated files, rewrite generated artifacts, or change CI/Docker layers unless required by the task.

## Public API documentation rule

This repository is a multi-language common / utilities repository. Any new
reusable public capability must be documented when it is added or changed.

A public capability includes, but is not limited to:

- public function
- public class
- public struct
- public enum
- public interface / trait
- public method intended for reuse
- package-level utility
- reusable script
- CLI entry

When adding or changing a public capability, update the corresponding language
directory `README.md`. The documentation update belongs in the language-specific
README, not only in source comments.

The root `README.md` should stay as a high-level navigation page. Detailed
function, class, type, and script usage belongs in the language-specific README,
for example:

- `python/README.md`
- `cpp/README.md`
- `golang/README.md`
- `rust/README.md`
- `dotnet/README.md`
- `java/flink-datastream-demo/README.md`

Each documented capability should include:

- category
- name
- type
- defined in
- import / include / use / package path
- purpose
- minimal usage example
- notes or caveats, if needed

Documentation should describe how to call and use the capability. Do not expose
implementation details. Do not copy internal source code into README files.
Private helpers, internal functions, test fixtures, generated files, build
outputs, and third-party dependency code should not be documented unless they
are intentionally reusable examples.

For Python and other scripting-language utilities, include REPL / interactive
console usage whenever documenting a public function or class.

Example:

```python
>>> from package.module import function_name
>>> function_name("input")
'output'
```

If it is unclear whether something should be treated as public reusable API,
document it as a candidate and mark it with `Needs review`.

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
