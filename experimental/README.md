# Experimental Playground

This directory is intentionally isolated from the repo-level Makefile; build and test only inside each subproject. Each language folder holds a minimal best-practice layout that extracts the substring `"world"` from the constant `"hello world"` and ships with a tiny test.

## Quick notes
- Keep experiments tidy so they can be promoted into language roots later; no artifacts are tracked thanks to the local `.gitignore`.
- Prefer per-project virtual envs/sandboxes: Python uses a venv, Node projects keep deps in their own `node_modules`, and other languages rely on their own build caches (Cargo target, Go module cache, etc.).
- Nothing here is wired into the top-level build; run the commands below from each subproject as needed.

## Commands (per language)
- C: `cmake -S . -B build && cmake --build build && ctest --test-dir build`
- C++: `cmake -S . -B build && cmake --build build && ctest --test-dir build`
- Rust: `cargo test`
- Java: `mvn test`
- Go: `go test ./...` (uses local `go.work` to stay isolated from the repo workspace)
- Python: `python -m venv .venv && source .venv/bin/activate && pip install .[dev] && pytest`
- Bash: `bash tests/hello_test.sh`
- Lua: `lua tests/hello_test.lua`
- JavaScript: `npm test`
- TypeScript: `npm install && npm test`
