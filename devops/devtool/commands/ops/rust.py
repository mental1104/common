from __future__ import annotations

import os
import re
import shlex
import shutil
import subprocess
from pathlib import Path
from typing import Mapping

from devtool.commands.common import RUST_DIR, run, strip_proxies


_CARGO_LLVM_COV_MIN_RUSTC = (1, 87, 0)
_CARGO_LLVM_COV_PINNED_VERSION = "0.6.21"


def _rustc_version(env: Mapping[str, str]) -> tuple[int, int, int] | None:
    try:
        out = subprocess.check_output(
            ["rustc", "--version"],
            env={k: str(v) for k, v in env.items()},
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    match = re.search(r"rustc\s+(\d+)\.(\d+)\.(\d+)", out)
    if not match:
        return None
    return tuple(int(x) for x in match.groups())


def _cargo_extra_args(env: Mapping[str, str]) -> list[str]:
    raw = env.get("CARGO_TEST_V", "")
    if not raw:
        return []
    try:
        return shlex.split(raw, posix=os.name != "nt")
    except ValueError:
        return raw.split()


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
    extra_args = _cargo_extra_args(env_np)
    base_args = ["cargo", "test", "--all-features"]
    if file_pattern:
        paths = [p.stem for p in (Path(RUST_DIR) / "tests").glob("*.rs") if p.match(file_pattern)]
        if paths:
            for b in paths:
                run_args = ["cargo", "test", "--all-features", "--test", b]
                if filter_expr:
                    run_args.append(filter_expr)
                run_args.extend(extra_args)
                run(run_args, env=env_np, cwd=RUST_DIR)
            return
    if filter_expr:
        base_args.append(filter_expr)
    base_args.extend(extra_args)
    run(base_args, env=env_np, cwd=RUST_DIR)


def bench(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    env_np = strip_proxies(env)
    extra_args = _cargo_extra_args(env_np)
    base_args = ["cargo", "bench"]
    if file_pattern:
        paths = [p.stem for p in (Path(RUST_DIR) / "benches").glob("*.rs") if p.match(file_pattern)]
        if paths:
            for b in paths:
                run_args = ["cargo", "bench", "--bench", b]
                if filter_expr:
                    run_args.append(filter_expr)
                run_args.extend(extra_args)
                run(run_args, env=env_np, cwd=RUST_DIR)
            return
    if filter_expr:
        base_args.append(filter_expr)
    base_args.extend(extra_args)
    run(base_args, env=env_np, cwd=RUST_DIR)


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
    if not shutil.which("cargo-llvm-cov", path=env_np.get("PATH")):
        llvm_cov_version = env.get("RUST_LLVM_COV_VERSION", "")
        if not llvm_cov_version:
            rustc_ver = _rustc_version(env_np)
            if rustc_ver and rustc_ver < _CARGO_LLVM_COV_MIN_RUSTC:
                llvm_cov_version = _CARGO_LLVM_COV_PINNED_VERSION
        cmd = ["cargo", "install", "cargo-llvm-cov"]
        if llvm_cov_version:
            cmd += ["--version", llvm_cov_version]
        run(cmd, env=env_np)
    ignore = "(^|/)(tests?|benches?|examples)/"
    run(["cargo", "llvm-cov", "--all-features", f"--ignore-filename-regex={ignore}", "--summary-only"], env=env_np, cwd=RUST_DIR)


def vet(env: Mapping[str, str]) -> None:
    run(["cargo", "clippy", "--all-targets", "--all-features"], env=env, cwd=RUST_DIR)


def guard(env: Mapping[str, str], *, mode: str | None = None) -> None:
    # Simplified guard: defer to sanitizers configured by user
    test(env, file_pattern=None, filter_expr=None)
