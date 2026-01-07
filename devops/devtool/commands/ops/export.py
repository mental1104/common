from __future__ import annotations

from pathlib import Path
from typing import Mapping

from devtool.commands.common import EXPORT_CPP_BUILD_DIR, ensure_dir, run
from devtool.commands.ops import cpp as cpp_ops


def build_export_cpp(env: Mapping[str, str]) -> None:
    # Ensure third-party libs (e.g., cJSON) are present without redoing work if already ready.
    cpp_ops.prepare_submodules(env, skip_when_ready=True)
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
