from __future__ import annotations

from pathlib import Path
import shutil
from typing import Mapping

from devtool.commands.common import EXPORT_CPP_BUILD_DIR, ensure_dir, run
from devtool.commands.ops import cpp as cpp_ops


def build_export_cpp(env: Mapping[str, str]) -> None:
    # Ensure third-party libs (e.g., cJSON) are present without redoing work if already ready.
    cpp_ops.prepare_submodules(env, skip_when_ready=True)
    cache = EXPORT_CPP_BUILD_DIR / "CMakeCache.txt"
    if cache.exists():
        expected_src = EXPORT_CPP_BUILD_DIR.parent.resolve()
        actual_src = ""
        for line in cache.read_text(errors="ignore").splitlines():
            if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
                actual_src = line.split("=", 1)[1].strip()
                break
        if actual_src and Path(actual_src).resolve() != expected_src:
            print(f"[warn] export/cpp build cache from {actual_src}, cleaning {EXPORT_CPP_BUILD_DIR}")
            shutil.rmtree(EXPORT_CPP_BUILD_DIR, ignore_errors=True)
    ensure_dir(EXPORT_CPP_BUILD_DIR)
    args = [
        env["CMAKE"],
        "-S",
        ".",
        "-B",
        str(EXPORT_CPP_BUILD_DIR),
        "-DEXPORT_BUILD_PYBIND11=ON",
        "-DPYBIND11_FINDPYTHON=ON",
        f'-DCMAKE_BUILD_TYPE={env["CPP_BUILD_TYPE"]}',
    ]
    pybind_dir = None
    venv_py = env.get("PY_VENV_PYTHON")
    if venv_py and Path(venv_py).exists():
        try:
            import pybind11  # type: ignore
        except Exception:
            pybind_dir = None
        else:
            try:
                pybind_dir = pybind11.get_cmake_dir()  # type: ignore[attr-defined]
            except Exception:
                pybind_dir = None
    if pybind_dir:
        args.append(f"-Dpybind11_DIR={pybind_dir}")
    run(args, env=env, cwd=Path("export") / "cpp")
    run([env["CMAKE"], "--build", str(EXPORT_CPP_BUILD_DIR)], env=cpp_ops.cmake_build_env(env))
