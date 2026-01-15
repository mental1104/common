from __future__ import annotations

import shutil
import subprocess
from pathlib import Path
from typing import Mapping

from devtool.commands.common import ROOT, run, strip_proxies

DOTNET_DIR = ROOT / "dotnet"
DOTNET_SOLUTION = DOTNET_DIR / "mental1104.sln"


def _dotnet(env: Mapping[str, str]) -> str:
    return env.get("DOTNET", "dotnet")


def _config(env: Mapping[str, str]) -> str:
    return env.get("DOTNET_CONFIGURATION", "Release")


def _is_truthy(value: str | None) -> bool:
    if value is None:
        return False
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def _ensure_solution() -> None:
    if not DOTNET_SOLUTION.exists():
        raise SystemExit(f"[error] missing solution file: {DOTNET_SOLUTION}")


def _maybe_logger(env: Mapping[str, str]) -> list[str]:
    logger = env.get("DOTNET_TEST_LOGGER", "")
    if not logger:
        return []
    return ["--logger", logger]

def _has_runtime(env: Mapping[str, str], major_prefix: str) -> bool:
    try:
        output = subprocess.check_output(
            [_dotnet(env), "--list-runtimes"],
            cwd=str(DOTNET_DIR),
            env={k: str(v) for k, v in env.items()},
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return False
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        if parts[0] != "Microsoft.NETCore.App":
            continue
        if parts[1].startswith(major_prefix):
            return True
    return False


def _prepare_test_env(env: Mapping[str, str]) -> dict[str, str]:
    env_np = strip_proxies(env)
    if "DOTNET_ROLL_FORWARD" in env_np:
        return env_np
    if _has_runtime(env_np, "8."):
        return env_np
    env_np = dict(env_np)
    env_np["DOTNET_ROLL_FORWARD"] = "LatestMajor"
    env_np.setdefault("DOTNET_ROLL_FORWARD_TO_PRERELEASE", "1")
    print("[warn] missing Microsoft.NETCore.App 8.x; enabling DOTNET_ROLL_FORWARD=LatestMajor for local runs")
    return env_np


def setup(env: Mapping[str, str]) -> None:
    _ensure_solution()
    if not _has_runtime(env, "8."):
        print("[warn] missing Microsoft.NETCore.App 8.x runtime; install .NET 8 for full-fidelity local tests")
    run([_dotnet(env), "restore", DOTNET_SOLUTION.name], env=env, cwd=DOTNET_DIR)


def build(env: Mapping[str, str]) -> None:
    _ensure_solution()
    run(
        [_dotnet(env), "build", DOTNET_SOLUTION.name, "--configuration", _config(env), "--no-restore"],
        env=env,
        cwd=DOTNET_DIR,
    )


def test(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    _ensure_solution()
    if file_pattern:
        print("[warn] dotnet test ignores --file; use --filter instead")
    env_np = _prepare_test_env(env)
    args = [_dotnet(env_np), "test", DOTNET_SOLUTION.name, "--configuration", _config(env_np)]
    if _is_truthy(env_np.get("DOTNET_TEST_NO_BUILD")):
        args.append("--no-build")
    if filter_expr:
        args += ["--filter", filter_expr]
    args += _maybe_logger(env_np)
    run(args, env=env_np, cwd=DOTNET_DIR)


def coverage(env: Mapping[str, str]) -> None:
    _ensure_solution()
    env_np = _prepare_test_env(env)
    args = [_dotnet(env_np), "test", DOTNET_SOLUTION.name, "--configuration", _config(env_np)]
    if _is_truthy(env_np.get("DOTNET_TEST_NO_BUILD")):
        args.append("--no-build")
    args += ["--collect:XPlat Code Coverage"]
    args += _maybe_logger(env_np)
    run(args, env=env_np, cwd=DOTNET_DIR)


def _dotnet_format_available(env: Mapping[str, str]) -> bool:
    if not shutil.which(_dotnet(env)):
        return False
    try:
        subprocess.check_output(
            [_dotnet(env), "format", "--version"],
            cwd=str(DOTNET_DIR),
            env={k: str(v) for k, v in env.items()},
            text=True,
        )
        return True
    except (OSError, subprocess.CalledProcessError):
        return False


def fmt(env: Mapping[str, str]) -> None:
    _ensure_solution()
    if not _dotnet_format_available(env):
        print("[warn] dotnet format is not available; skipping")
        return
    run([_dotnet(env), "format"], env=env, cwd=DOTNET_DIR)


def bench(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    if not filter_expr and not file_pattern:
        print("[warn] dotnet bench is not configured; provide --filter to run")
        return
    test(env, file_pattern=file_pattern, filter_expr=filter_expr)


def clean(env: Mapping[str, str]) -> None:
    def _manual_clean() -> None:
        for name in ("bin", "obj", "TestResults"):
            for p in DOTNET_DIR.rglob(name):
                if p.is_dir():
                    shutil.rmtree(p, ignore_errors=True)

    if DOTNET_SOLUTION.exists():
        allow_fail = _is_truthy(env.get("DOTNET_CLEAN_ALLOW_FAIL"))
        failed = False
        try:
            run(
                [
                    _dotnet(env),
                    "clean",
                    DOTNET_SOLUTION.name,
                    "--configuration",
                    _config(env),
                ],
                env=env,
                cwd=DOTNET_DIR,
            )
        except (OSError, subprocess.CalledProcessError):
            if not allow_fail:
                raise
            failed = True
            print("[warn] dotnet clean failed; falling back to manual cleanup")
        _manual_clean()
        if failed:
            return
    else:
        _manual_clean()


def install(env: Mapping[str, str]) -> None:
    print("[warn] dotnet install is not configured; skipping")


def uninstall(env: Mapping[str, str]) -> None:
    print("[warn] dotnet uninstall is not configured; skipping")


def vet(env: Mapping[str, str]) -> None:
    build(env)


def guard(env: Mapping[str, str]) -> None:
    test(env, file_pattern=None, filter_expr=None)
