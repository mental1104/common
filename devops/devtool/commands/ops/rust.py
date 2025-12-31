from __future__ import annotations

import os
import shutil
from pathlib import Path
from typing import Mapping

from devtool.commands.common import RUST_DIR, run, strip_proxies


def setup(env: Mapping[str, str]) -> None:
    if not shutil.which("cargo"):
        raise SystemExit("[error] 未找到 cargo")
    if (Path(RUST_DIR) / "rust-toolchain.toml").exists():
        run(["rustup", "toolchain", "install", "stable"], env=env)
        run(["rustup", "override", "set", "stable"], env=env, cwd=RUST_DIR)
    run(["cargo", "fetch"], env=env, cwd=RUST_DIR)


def build(env: Mapping[str, str]) -> None:
    run(["cargo", "build", "--release"], env=env, cwd=RUST_DIR)


def test(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    env_np = strip_proxies(env)
    args = ["cargo", "test", "--all-features"]
    if env.get("CARGO_TEST_V"):
        args.append(env["CARGO_TEST_V"])
    if filter_expr:
        args.append(filter_expr)
    if file_pattern:
        paths = [p.stem for p in (Path(RUST_DIR) / "tests").glob("*.rs") if p.match(file_pattern)]
        if paths:
            for b in paths:
                run(args + ["--test", b], env=env_np, cwd=RUST_DIR)
            return
    run(args, env=env_np, cwd=RUST_DIR)


def bench(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    env_np = strip_proxies(env)
    args = ["cargo", "bench"]
    if env.get("CARGO_TEST_V"):
        args.append(env["CARGO_TEST_V"])
    if filter_expr:
        args.append(filter_expr)
    if file_pattern:
        paths = [p.stem for p in (Path(RUST_DIR) / "benches").glob("*.rs") if p.match(file_pattern)]
        if paths:
            for b in paths:
                run(args + ["--bench", b], env=env_np, cwd=RUST_DIR)
            return
    run(args, env=env_np, cwd=RUST_DIR)


def fmt(env: Mapping[str, str]) -> None:
    run(["cargo", "fmt", "--all"], env=strip_proxies(env), cwd=RUST_DIR)


def clippy(env: Mapping[str, str]) -> None:
    run(["cargo", "clippy", "--all-targets", "--all-features", "--", "-D", "warnings"], env=strip_proxies(env), cwd=RUST_DIR)


def example(env: Mapping[str, str]) -> None:
    run(["cargo", "run", "--example", "contains"], env=strip_proxies(env), cwd=RUST_DIR)


def clean(env: Mapping[str, str]) -> None:
    run(["cargo", "clean"], env=strip_proxies(env), cwd=RUST_DIR)
    for pattern in ("coverage", "flamegraph.svg", "perf.data*"):
        for p in Path(RUST_DIR).glob(pattern):
            if p.is_dir():
                shutil.rmtree(p, ignore_errors=True)
            else:
                p.unlink(missing_ok=True)


def install(env: Mapping[str, str]) -> None:
    cargo_toml = Path(RUST_DIR) / "Cargo.toml"
    if cargo_toml.read_text().find("[[bin]]") != -1 or (Path(RUST_DIR) / "src/main.rs").exists():
        run(["cargo", "install", "--path", ".", "--locked", "--force"], env=strip_proxies(env), cwd=RUST_DIR)
    else:
        build(env)


def uninstall(env: Mapping[str, str]) -> None:
    run(["cargo", "uninstall", "mental1104"], env=strip_proxies(env), cwd=RUST_DIR)


def coverage(env: Mapping[str, str]) -> None:
    env_np = strip_proxies(env)
    run(["rustup", "component", "add", "llvm-tools-preview"], env=env_np)
    if not shutil.which("cargo-llvm-cov"):
        run(["cargo", "install", "cargo-llvm-cov"], env=env_np)
    ignore = "(^|/)(tests?|benches?|examples)/"
    run(["cargo", "llvm-cov", "--all-features", f"--ignore-filename-regex={ignore}", "--summary-only"], env=env_np, cwd=RUST_DIR)


def vet(env: Mapping[str, str]) -> None:
    run(["cargo", "clippy", "--all-targets", "--all-features"], env=env, cwd=RUST_DIR)


def guard(env: Mapping[str, str], *, mode: str | None = None) -> None:
    # Simplified guard: defer to sanitizers configured by user
    test(env, file_pattern=None, filter_expr=None)
