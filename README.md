# common

## What is this repository?

`common` is a multi-language utilities and reusable snippets repository. It keeps small, reusable capabilities for Python, C++, Go, Rust, .NET, and Java in one workspace, with `./dev` commands wrapping the usual build, test, coverage, formatting, and install workflows.

This README is the navigation entry for caller-facing usage documentation. It does not expand every function or class; each language README owns the detailed callable API index and minimal usage examples.

## Language indexes

| Language | Path | What is inside |
|---|---|---|
| Python | [./python/](./python/) | Package utilities for serialization, files, app helpers, async/concurrency, DB/Redis/Mongo scopes, MQ connectors, ASGI/FastAPI context, i18n, plotting, environment checks, and reusable scripts. |
| C++ | [./cpp/](./cpp/) | Public headers under `cpp/include/mental1104` for containers, caches, logging, JSON views, RAII wrappers, concurrency helpers, random keys, semantic base types, stacktrace C API, Redis lock, and network/event utilities. |
| Go | [./golang/](./golang/) | `github.com/mental1104/common/golang` with reusable containment helpers plus runnable examples and lab commands. |
| Rust | [./rust/](./rust/) | `mental1104` crate with collection containment helpers, sorted-slice wrapper, prelude exports, and common error/result aliases. |
| .NET | [./dotnet/](./dotnet/) | `Mental1104` library utilities, currently focused on executable file validation. |
| Java | [./java/](./java/) | Maven-based Flink demo plus reusable Java containment helpers. |

## Documentation rule

When adding a new public function, class, struct, enum, interface, trait, reusable method, script, CLI entry, or package-level utility, update the corresponding language README in the same change.

Each new entry must include:

1. category
2. name
3. short purpose
4. import/include/package path
5. minimal usage example
6. notes or caveats

Python entries must also include REPL usage. If a symbol is public in code but its intended stability is unclear, document it as a candidate and add `Needs review` in the notes.

The older lowercase [readme.md](./readme.md) keeps historical dev command and coverage details. Use this `README.md` as the capability index entry point.
