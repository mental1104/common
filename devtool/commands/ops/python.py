from __future__ import annotations

import os
import shutil
from pathlib import Path
from typing import Iterable, Mapping

from devtool.commands.common import (
    BENCH_ARTIFACT_ROOT,
    CPP_BUILD_DIR,
    EXPORT_CPP_BUILD_DIR,
    PY_DIR,
    PY_VENV,
    PY_VENV_PIP,
    PY_VENV_PYTHON,
    ensure_dir,
    run,
    strip_proxies,
)


def _venv_env(env: Mapping[str, str]) -> dict[str, str]:
    venv_env = dict(env)
    path_sep = os.pathsep
    venv_bin = str(Path(PY_VENV_PYTHON).parent)
    venv_env["VIRTUAL_ENV"] = str(PY_VENV)
    venv_env["PATH"] = venv_bin + path_sep + env.get("PATH", "")
    return venv_env


def _ensure_venv(env: Mapping[str, str]) -> Mapping[str, str]:
    py = env.get("PYTHON", "python3")
    venv_py = Path(env["PY_VENV_PYTHON"])
    if not venv_py.exists():
        ensure_dir(PY_VENV)
        run([py, "-m", "venv", str(PY_VENV)], env=env)
    return _venv_env(env)


def _upgrade_build_tools(env: Mapping[str, str]) -> None:
    run([env["PY_VENV_PIP"], "install", "--no-build-isolation", "--upgrade", "pip", "setuptools", "wheel"], env=env)


def _install_export_layer(env: Mapping[str, str]) -> None:
    if (Path("export") / "python").is_dir():
        run([env["PY_VENV_PIP"], "install", "--no-build-isolation", "-e", "."], env=env, cwd=Path("export") / "python")


def _install_requirements(env: Mapping[str, str]) -> None:
    req_file = PY_DIR / "requirements.txt"
    export_dir = Path("export/python")
    if not req_file.exists():
        return
    if not export_dir.exists():
        raise SystemExit(f"[err] 缺少 export/python 目录：{export_dir}")
    text = req_file.read_text()
    bak = req_file.with_suffix(req_file.suffix + ".bak.setup")
    replaced = False
    if "file://../export/python" in text:
        bak.write_text(text)
        req_file.write_text(text.replace("file://../export/python", f"file://{export_dir.resolve()}"))
        replaced = True
    try:
        run([env["PY_VENV_PIP"], "install", "--no-build-isolation", "-r", str(req_file)], env=env, cwd=PY_DIR)
    finally:
        if replaced:
            req_file.write_text(text)
            if bak.exists():
                bak.unlink(missing_ok=True)


def _generate_init(env: Mapping[str, str]) -> None:
    script = PY_DIR / "generate_init.py"
    if script.exists():
        run([env["PY_VENV_PYTHON"], str(script)], env=env)


def _fix_future_annotations() -> None:
    # Lightweight insertion: add future import after shebang/comments if "|" appears in annotations and import missing.
    for path in PY_DIR.rglob("*.py"):
        if ".venv" in path.parts:
            continue
        text = path.read_text()
        if "|" not in text or "from __future__ import annotations" in text:
            continue
        lines = text.splitlines()
        insert_at = 0
        for idx, line in enumerate(lines):
            if line.startswith("#!"):
                insert_at = idx + 1
                continue
            stripped = line.strip()
            if stripped.startswith("#") or stripped == "":
                continue
            insert_at = idx
            break
        lines.insert(insert_at, "from __future__ import annotations")
        path.write_text("\n".join(lines) + "\n")


def _build_wheel(env: Mapping[str, str]) -> None:
    ensure_dir(PY_DIR / "dist")
    run([env["PY_VENV_PYTHON"], "-m", "pip", "wheel", "--no-deps", "-w", "dist", "."], env=env, cwd=PY_DIR)


def setup(env: Mapping[str, str]) -> None:
    venv_env = _ensure_venv(env)
    _upgrade_build_tools(venv_env)
    _install_export_layer(venv_env)
    _install_requirements(venv_env)
    _generate_init(venv_env)
    _fix_future_annotations()
    _build_wheel(venv_env)


def build(env: Mapping[str, str]) -> None:
    venv_env = _ensure_venv(env)
    _build_wheel(venv_env)


def _ensure_pytest_benchmark(env: Mapping[str, str]) -> None:
    run([env["PY_VENV_PIP"], "show", "pytest-benchmark"], env=env)
    # If show passes, do nothing; if fails, install


def test(
    env: Mapping[str, str],
    *,
    pytest_args: Iterable[str],
    file_pattern: str | None,
    filter_expr: str | None,
    runner: str | None = None,
) -> None:
    venv_env = _ensure_venv(env)
    # ensure pytest-benchmark
    try:
        run([venv_env["PY_VENV_PIP"], "show", "pytest-benchmark"], env=venv_env)
    except Exception:
        run([venv_env["PY_VENV_PIP"], "install", "pytest-benchmark"], env=venv_env)
    extra_env = strip_proxies(venv_env)
    extra_env["EXPORT_LAYER_LOG_LEVEL"] = "INFO"
    exp_lib = None
    for ext in ("so", "dylib", "dll"):
        cand = Path(EXPORT_CPP_BUILD_DIR) / f"libexport_json.{ext}"
        if cand.exists():
            exp_lib = cand
            break
    if exp_lib:
        extra_env["EXPORT_LAYER_CTYPE_LIB"] = str(exp_lib)
    extra_env["PYTHONPATH"] = str(EXPORT_CPP_BUILD_DIR) + os.pathsep + extra_env.get("PYTHONPATH", "")
    extra_env["PYTEST_DISABLE_PLUGIN_AUTOLOAD"] = "1"
    extra_env["PYTEST_PLUGINS"] = "pytest_benchmark.plugin,pytest_asyncio.plugin,pytest_mock"
    kexpr = filter_expr or ""
    if file_pattern:
        kexpr = f"({file_pattern})" if not kexpr else f"({file_pattern}) and ({kexpr})"
    default_k = extra_env.get("KEXPR_DEFAULT", "not bench and not benchmark")
    kexpr = f"{default_k}" if not kexpr else f"{kexpr} and {default_k}"
    args = [extra_env["PY_VENV_PYTHON"], "-m", "pytest"]
    if extra_env.get("PYTEST_V"):
        args.append(extra_env["PYTEST_V"])
    args.extend(pytest_args)
    args.extend(["-k", kexpr])
    run(args, env=extra_env, cwd=PY_DIR)


def coverage(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    venv_env = _ensure_venv(env)
    extra_env = strip_proxies(venv_env)
    extra_env["PYTEST_DISABLE_PLUGIN_AUTOLOAD"] = "1"
    extra_env["PYTEST_PLUGINS"] = "pytest_benchmark.plugin,pytest_asyncio.plugin,pytest_mock"
    extra_env["EXPORT_LAYER_LOG_LEVEL"] = "INFO"
    exp_lib = None
    for ext in ("so", "dylib", "dll"):
        cand = Path(EXPORT_CPP_BUILD_DIR) / f"libexport_json.{ext}"
        if cand.exists():
            exp_lib = cand
            break
    if exp_lib:
        extra_env["EXPORT_LAYER_CTYPE_LIB"] = str(exp_lib)
    extra_env["PYTHONPATH"] = str(EXPORT_CPP_BUILD_DIR) + os.pathsep + extra_env.get("PYTHONPATH", "")
    kexpr = filter_expr or ""
    if file_pattern:
        kexpr = f"({file_pattern})" if not kexpr else f"({file_pattern}) and ({kexpr})"
    kexpr = kexpr or "not bench and not benchmark"
    args = [
        extra_env["PY_VENV_PYTHON"],
        "-m",
        "coverage",
        "run",
        "--source=.",
        "-m",
        "pytest",
        "-c",
        "/dev/null",
        "-k",
        kexpr,
    ]
    run(args, env=extra_env, cwd=PY_DIR)
    run([extra_env["PY_VENV_PYTHON"], "-m", "coverage", "report"], env=extra_env, cwd=PY_DIR)


def fmt(env: Mapping[str, str]) -> None:
    venv_env = _ensure_venv(env)
    try:
        run([venv_env["PY_VENV_PYTHON"], "-c", "import autopep8"], env=venv_env)
    except Exception:
        run([venv_env["PY_VENV_PIP"], "install", "autopep8"], env=venv_env)
    run(
        [
            venv_env["PY_VENV_PYTHON"],
            "-m",
            "autopep8",
            "--in-place",
            "--recursive",
            "--max-line-length=120",
            "--ignore=E402,E226,E24,W50,W690",
            ".",
        ],
        env=venv_env,
        cwd=PY_DIR,
    )


def bench(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    venv_env = _ensure_venv(env)
    artifact_dir = Path(BENCH_ARTIFACT_ROOT) / "python"
    ensure_dir(artifact_dir)
    extra_env = strip_proxies(venv_env)
    extra_env["PYTEST_DISABLE_PLUGIN_AUTOLOAD"] = "1"
    extra_env["PYTEST_PLUGINS"] = "pytest_benchmark.plugin"
    kexpr = filter_expr or extra_env.get("PYTEST_BENCH_K", "bench or benchmark")
    if file_pattern:
        kexpr = f"({file_pattern}) and ({kexpr})"
    args = [extra_env["PY_VENV_PYTHON"], "-m", "pytest"]
    if extra_env.get("PYTEST_V"):
        args.append(extra_env["PYTEST_V"])
    args.extend(["-k", kexpr, "--benchmark-only"])
    run(args, env=extra_env, cwd=PY_DIR)


def install(env: Mapping[str, str]) -> None:
    run_env = dict(env)
    wheel_candidates = list((PY_DIR / "dist").glob("*.whl"))
    break_flag = env.get("BREAK_FLAG", "")
    pip3 = env.get("PIP3", "pip3")
    run([pip3, "install", "--upgrade", "pip", "setuptools", "wheel", *break_flag.split()], env=run_env)
    export_dir = ROOT / "export" / "python"
    if wheel_candidates:
        wheel = sorted(wheel_candidates)[-1]
        if export_dir.exists():
            run([pip3, "install", str(export_dir), "--no-build-isolation", "--no-deps", *break_flag.split()], env=run_env)
        run([pip3, "install", str(wheel), "--no-deps", *break_flag.split()], env=run_env)
    else:
        if export_dir.exists():
            run([pip3, "install", str(export_dir), "--no-build-isolation", "--no-deps", *break_flag.split()], env=run_env)
        run([pip3, "install", str(PY_DIR), "--upgrade", "--no-build-isolation", "--no-deps", *break_flag.split()], env=run_env)


def uninstall(env: Mapping[str, str]) -> None:
    pip3 = env.get("PIP3", "pip3")
    break_flag = env.get("BREAK_FLAG", "")
    for pkg in ("mental1104_export_layer", "mental1104-export-layer", "mental1104"):
        run([pip3, "uninstall", "-y", pkg, *break_flag.split()], env=env)


def clean(env: Mapping[str, str]) -> None:
    shutil.rmtree(PY_DIR / "build", ignore_errors=True)
    shutil.rmtree(PY_DIR / "dist", ignore_errors=True)
    shutil.rmtree(PY_DIR / ".venv", ignore_errors=True)
    for pattern in ["*.egg-info", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".benchmarks"]:
        for p in PY_DIR.glob(pattern):
            if p.is_dir():
                shutil.rmtree(p, ignore_errors=True)
            else:
                p.unlink(missing_ok=True)
    for path in PY_DIR.rglob("__pycache__"):
        shutil.rmtree(path, ignore_errors=True)
    for path in PY_DIR.rglob("*.py[co]"):
        path.unlink(missing_ok=True)


def vet(env: Mapping[str, str]) -> None:
    venv_env = _ensure_venv(env)
    try:
        run([venv_env["PY_VENV_PYTHON"], "-c", "import ruff"], env=venv_env)
    except Exception:
        run([venv_env["PY_VENV_PIP"], "install", "ruff"], env=venv_env)
    run([venv_env["PY_VENV_PYTHON"], "-m", "ruff", "check", "--select", "F,B,UP,PERF", "mental1104"], env=venv_env, cwd=PY_DIR)


def guard(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    vet(env)
    test(env, pytest_args=[], file_pattern=file_pattern, filter_expr=filter_expr)
