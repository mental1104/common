from __future__ import annotations

import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Mapping

from devtool.commands.common import RUST_DIR, run, strip_proxies

_CARGO_LLVM_COV_MIN_RUSTC = (1, 87, 0)
_CARGO_LLVM_COV_FALLBACK_VERSIONS = ("0.6.20", "0.6.19", "0.6.18")


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


def _install_cargo_llvm_cov(env: Mapping[str, str], versions: list[str]) -> None:
    last_exc: Exception | None = None
    for version in versions:
        for locked in (True, False):
            cmd = ["cargo", "install", "cargo-llvm-cov"]
            if version:
                cmd += ["--version", version]
            if locked:
                cmd.append("--locked")
            try:
                run(cmd, env=env)
                return
            except Exception as exc:
                last_exc = exc
                if locked:
                    continue
                if version != versions[-1]:
                    print(f"[warn] cargo-llvm-cov {version} install failed; trying older version")
    if last_exc:
        raise last_exc


def _rustup_which(env: Mapping[str, str], tool: str) -> str:
    try:
        path = subprocess.check_output(
            ["rustup", "which", tool],
            env={k: str(v) for k, v in env.items()},
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""
    if not path:
        return ""
    p = Path(path)
    return str(p) if p.is_file() else ""


def _rustc_sysroot(env: Mapping[str, str]) -> str:
    try:
        return subprocess.check_output(
            ["rustc", "--print", "sysroot"],
            env={k: str(v) for k, v in env.items()},
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def _rustc_host(env: Mapping[str, str]) -> str:
    try:
        out = subprocess.check_output(
            ["rustc", "-vV"],
            env={k: str(v) for k, v in env.items()},
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return ""
    for line in out.splitlines():
        if line.startswith("host:"):
            return line.split(":", 1)[1].strip()
    return ""


def _rustc_tool_from_sysroot(env: Mapping[str, str], tool: str) -> str:
    sysroot = _rustc_sysroot(env)
    host = _rustc_host(env)
    if not sysroot or not host:
        return ""
    cand = Path(sysroot) / "lib" / "rustlib" / host / "bin" / tool
    return str(cand) if cand.is_file() else ""


def _rustup_run_tool(env: Mapping[str, str], args: list[str]) -> str:
    if not shutil.which("rustup"):
        return ""
    toolchain = (env.get("RUSTUP_TOOLCHAIN") or "").strip() or "stable"
    try:
        return subprocess.check_output(
            ["rustup", "run", toolchain] + args,
            env={k: str(v) for k, v in env.items()},
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def _rustup_tool_from_sysroot(env: Mapping[str, str], tool: str) -> str:
    sysroot = _rustup_run_tool(env, ["rustc", "--print", "sysroot"])
    if not sysroot:
        return ""
    host = ""
    rustc_v = _rustup_run_tool(env, ["rustc", "-vV"])
    for line in rustc_v.splitlines():
        if line.startswith("host:"):
            host = line.split(":", 1)[1].strip()
            break
    if not host:
        return ""
    cand = Path(sysroot) / "lib" / "rustlib" / host / "bin" / tool
    return str(cand) if cand.is_file() else ""


def _is_truthy(value: str | None) -> bool:
    if value is None:
        return False
    return str(value).strip().lower() in ("1", "true", "yes", "on")
def _xcrun_find(env: Mapping[str, str], tool: str) -> str:
    if os.name != "posix" or not sys.platform.startswith("darwin"):
        return ""
    try:
        path = subprocess.check_output(
            ["xcrun", "--find", tool],
            env={k: str(v) for k, v in env.items()},
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""
    if not path:
        return ""
    p = Path(path)
    return str(p) if p.is_file() else ""


def _brew_llvm_bin(env: Mapping[str, str], tool: str) -> str:
    if not shutil.which("brew"):
        return ""
    try:
        prefix = subprocess.check_output(
            ["brew", "--prefix", "llvm"],
            env={k: str(v) for k, v in env.items()},
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""
    if not prefix:
        return ""
    cand = Path(prefix) / "bin" / tool
    return str(cand) if cand.is_file() else ""


def _resolve_llvm_tool(env: Mapping[str, str], tool: str, env_key: str) -> str:
    provided = env.get(env_key, "")
    if provided and Path(provided).is_file():
        return provided
    sysroot_path = _rustc_tool_from_sysroot(env, tool)
    if sysroot_path:
        return sysroot_path
    rustup_sysroot_path = _rustup_tool_from_sysroot(env, tool)
    if rustup_sysroot_path:
        return rustup_sysroot_path
    rustup_path = _rustup_which(env, tool)
    if rustup_path:
        return rustup_path
    which_path = shutil.which(tool)
    if which_path and Path(which_path).is_file():
        return which_path
    xcrun_path = _xcrun_find(env, tool)
    if xcrun_path:
        return xcrun_path
    brew_path = _brew_llvm_bin(env, tool)
    if brew_path:
        return brew_path
    return ""


def _ensure_llvm_tools(env: Mapping[str, str]) -> dict[str, str]:
    env_np = dict(env)
    if not env_np.get("LLVM_COV") or not env_np.get("LLVM_PROFDATA"):
        last_exc: Exception | None = None
        for comp in ("llvm-tools-preview", "llvm-tools"):
            try:
                run(["rustup", "component", "add", comp], env=env_np)
                last_exc = None
                break
            except Exception as exc:
                last_exc = exc
        if last_exc:
            print(f"[warn] rustup component add llvm-tools failed: {last_exc}")
    llvm_cov = _resolve_llvm_tool(env_np, "llvm-cov", "LLVM_COV")
    if llvm_cov:
        env_np["LLVM_COV"] = llvm_cov
    llvm_profdata = _resolve_llvm_tool(env_np, "llvm-profdata", "LLVM_PROFDATA")
    if llvm_profdata:
        env_np["LLVM_PROFDATA"] = llvm_profdata
    return env_np


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
    env_np = _ensure_llvm_tools(env_np)
    if not shutil.which("cargo-llvm-cov", path=env_np.get("PATH")):
        llvm_cov_version = env.get("RUST_LLVM_COV_VERSION", "")
        if llvm_cov_version:
            versions = [llvm_cov_version]
        else:
            rustc_ver = _rustc_version(env_np)
            if rustc_ver and rustc_ver < _CARGO_LLVM_COV_MIN_RUSTC:
                versions = list(_CARGO_LLVM_COV_FALLBACK_VERSIONS)
            else:
                versions = [""]
        _install_cargo_llvm_cov(env_np, versions)
    ignore = "(^|/)(tests?|benches?|examples)/"
    if _is_truthy(env_np.get("RUST_COVER_XML")):
        xml_out = env_np.get("RUST_COVER_XML_PATH", "coverage.xml")
        run(
            [
                "cargo",
                "llvm-cov",
                "--all-features",
                f"--ignore-filename-regex={ignore}",
                "--cobertura",
                "--output-path",
                xml_out,
            ],
            env=env_np,
            cwd=RUST_DIR,
        )
    else:
        run(
            ["cargo", "llvm-cov", "--all-features", f"--ignore-filename-regex={ignore}", "--summary-only"],
            env=env_np,
            cwd=RUST_DIR,
        )


def vet(env: Mapping[str, str]) -> None:
    run(["cargo", "clippy", "--all-targets", "--all-features"], env=env, cwd=RUST_DIR)


def guard(env: Mapping[str, str], *, mode: str | None = None) -> None:
    _ = mode
    # Simplified guard: defer to sanitizers configured by user
    test(env, file_pattern=None, filter_expr=None)
