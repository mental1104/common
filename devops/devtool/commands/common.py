from __future__ import annotations

import os
from pathlib import Path
from typing import Dict, Mapping, Sequence

from devtool import config as dev_config
from devtool.context import ROOT, is_windows, sh

PROXY_KEYS = ("HTTP_PROXY", "HTTPS_PROXY", "NO_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "no_proxy", "all_proxy")


def _parse_env_file(path: Path) -> Dict[str, str]:
    env: Dict[str, str] = {}
    if not path.exists():
        return env
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[len("export ") :].strip()
        if "=" not in line:
            continue
        key, val = line.split("=", 1)
        env[key.strip()] = val.strip().strip('"').strip("'")
    return env


def _venv_bin(root: Path) -> Path:
    return root / ("Scripts" if is_windows() else "bin")


DEVTOOL_DIR = Path(__file__).resolve().parents[1]
PY_DIR = ROOT / "python"
GO_DIR = ROOT / "golang"
CPP_SRC_DIR = ROOT / "cpp"
CPP_BUILD_DIR = CPP_SRC_DIR / "build"
EXPORT_CPP_BUILD_DIR = ROOT / "export" / "cpp" / "build"
RUST_DIR = ROOT / "rust" / "mental1104"
BENCH_ARTIFACT_ROOT = ROOT / "artifacts" / "bench"
CPP_BENCH_ARTIFACT_DIR = BENCH_ARTIFACT_ROOT / "cpp"
BOOST_SPARSE_LIST = DEVTOOL_DIR / "boost_sparse_checkout.txt"
BOOST_REQUIRED_SUBMODULES = DEVTOOL_DIR / "boost_required_submodules.txt"
PY_VENV = PY_DIR / ".venv"
PY_VENV_BIN = _venv_bin(PY_VENV)
PY_VENV_PYTHON = PY_VENV_BIN / ("python.exe" if is_windows() else "python")
PY_VENV_PIP = PY_VENV_BIN / ("pip.exe" if is_windows() else "pip")


def default_jobs() -> int:
    return os.cpu_count() or 4


def base_env(
    verbose: bool = False,
    test_verbose: bool | None = None,
    jobs: int | None = None,
    cpp_build_type: str | None = None,
    build_verbose: bool | None = None,
    prefix: str | None = None,
) -> Dict[str, str]:
    env: Dict[str, str] = os.environ.copy()
    env_src = ROOT / ".env"
    if env_src.exists():
        env.update(_parse_env_file(env_src))
    jobs_val = jobs if jobs and jobs > 0 else default_jobs()
    try:
        env_jobs = int(env.get("JOBS", ""))
        if env_jobs > 0 and jobs is None:
            jobs_val = env_jobs
    except ValueError:
        pass
    test_v = verbose if test_verbose is None else test_verbose
    env.setdefault("PYTHON", "py" if is_windows() else "python3")
    env.setdefault("PIP3", "pip" if is_windows() else "pip3")
    env.setdefault("CMAKE", "cmake")
    env.setdefault("CTEST", "ctest")
    env.setdefault("GCOV", env.get("GCOV", "gcov"))
    env.setdefault("GO", "go")
    env.setdefault("DOTNET", "dotnet")
    uid = env.get("UID")
    if uid is None:
        try:
            uid_val = os.geteuid()
        except AttributeError:
            uid_val = None
        uid = "" if uid_val is None else str(uid_val)
    env.setdefault("UID", str(uid))
    env.setdefault("SUDO", "" if str(uid) == "0" or is_windows() else "sudo")
    env.setdefault("JOBS", str(jobs_val))
    env["CPP_BUILD_TYPE"] = cpp_build_type or env.get("CPP_BUILD_TYPE") or "Debug"
    env["PREFIX"] = prefix or env.get("PREFIX") or (str(Path.home() / ".local") if is_windows() else "/usr/local")
    env.setdefault("VERBOSE", "1" if verbose else "0")
    build_verbose_val = build_verbose
    if build_verbose_val is None:
        raw_build_verbose = env.get("BUILD_VERBOSE")
        if raw_build_verbose is not None:
            build_verbose_val = str(raw_build_verbose).strip().lower() in ("1", "true", "yes", "on")
        else:
            build_verbose_val = verbose
    env["BUILD_VERBOSE"] = "1" if build_verbose_val else "0"
    env.setdefault("TEST_VERBOSE", "1" if test_v else "0")
    env.setdefault("CTEST_V", "-V" if env["TEST_VERBOSE"] == "1" else "")
    env.setdefault("PYTEST_V", "-vv" if env["TEST_VERBOSE"] == "1" else "-q")
    env.setdefault("GO_TEST_V", "-v" if env["TEST_VERBOSE"] == "1" else "")
    env.setdefault("CARGO_TEST_V", "-v" if env["TEST_VERBOSE"] == "1" else "")
    env.setdefault("DOTNET_CONFIGURATION", env.get("DOTNET_CONFIGURATION", "Release"))
    env.setdefault("DOTNET_TEST_NO_BUILD", env.get("DOTNET_TEST_NO_BUILD", "0"))
    env.setdefault("PY_BENCHMARK_OPTS", "--benchmark-name=short --benchmark-sort=name")
    env.setdefault("PY_BENCHMARK_CONCURRENCY_OPTS", "--benchmark-max-time=0.25 --benchmark-min-rounds=3")
    env.setdefault("PYTEST_BENCH_K", "bench or benchmark")
    env.setdefault("COMPOSE_BIN", env.get("COMPOSE_BIN", "docker compose"))
    env.setdefault("COMPOSE_FILE_NAME", env.get("COMPOSE_FILE_NAME", "docker-compose.yaml"))
    break_flag = env.get("BREAK_FLAG")
    if break_flag is None:
        is_ubuntu = False
        os_release = Path("/etc/os-release")
        if os_release.exists():
            data = os_release.read_text()
            if "ID=ubuntu" in data or 'ID="ubuntu"' in data:
                is_ubuntu = True
        break_flag = "--break-system-packages" if is_ubuntu else ""
    env["BREAK_FLAG"] = break_flag
    env.setdefault("REPO_ROOT", str(ROOT))
    env.setdefault("PY_VENV", str(PY_VENV))
    env.setdefault("PY_VENV_PYTHON", str(PY_VENV_PYTHON))
    env.setdefault("PY_VENV_PIP", str(PY_VENV_PIP))
    env.setdefault("GO_DIR", str(GO_DIR))
    env.setdefault("CPP_SRC_DIR", str(CPP_SRC_DIR))
    env.setdefault("CPP_BUILD_DIR", str(CPP_BUILD_DIR))
    env.setdefault("EXPORT_CPP_BUILD_DIR", str(EXPORT_CPP_BUILD_DIR))
    env.setdefault("RUST_DIR", str(RUST_DIR))
    env.setdefault("BENCH_ARTIFACT_ROOT", str(BENCH_ARTIFACT_ROOT))
    env.setdefault("PY_BENCH_ARTIFACT_DIR", str(BENCH_ARTIFACT_ROOT / "python"))
    env.setdefault("CPP_BENCH_ARTIFACT_DIR", str(BENCH_ARTIFACT_ROOT / "cpp"))
    env.setdefault("GOBIN", env.get("GOBIN", ""))
    env.setdefault("GOWORK", env.get("GOWORK", "off"))
    env.setdefault("GOPROXY", env.get("GOPROXY", ""))
    env.setdefault("GOPRIVATE", env.get("GOPRIVATE", ""))
    env.setdefault("GOTOOLCHAIN", env.get("GOTOOLCHAIN", "local"))
    dev_config.apply_pip_mirror_env(env)
    env.setdefault("RUST_COVER_FAIL_UNDER", env.get("RUST_COVER_FAIL_UNDER", ""))
    env.setdefault("VET_DIR", env.get("VET_DIR", "cpp/test"))
    return env


def strip_proxies(env: Mapping[str, str]) -> Dict[str, str]:
    new_env = dict(env)
    for key in PROXY_KEYS:
        new_env.pop(key, None)
    return new_env


def run(cmd: Sequence[str] | str, env: Mapping[str, str], cwd: Path | None = None) -> None:
    sh(cmd, cwd=cwd or ROOT, env=env)


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
