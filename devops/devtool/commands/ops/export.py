"""复用 C++ 依赖准备流程，配置并构建可选的 Python 导出库。

构建缓存必须属于当前源码目录；可导入的 pybind11 提供 CMake 查找路径，
扩展不可用时由导出工程的可选构建逻辑处理。
"""

from __future__ import annotations

from pathlib import Path
import shutil
from typing import Mapping

from devtool.commands.common import EXPORT_CPP_BUILD_DIR, ensure_dir, run
from devtool.commands.ops import cpp as cpp_ops


def build_export_cpp(env: Mapping[str, str]) -> None:
    """准备缺失的子模块，并在既有导出构建目录配置和编译。

    Args:
        env: 只读命令环境，包含 CMAKE、CPP_BUILD_TYPE，以及可选的 PY_VENV_PYTHON。
    Returns:
        无返回值；更新导出库产物，源码路径不匹配的旧缓存会被清理。
    Raises:
        subprocess.CalledProcessError: 依赖准备、CMake 配置或构建失败。
    """
    # 复用已准备的源码，避免为导出层重新初始化全部依赖。
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
    # 传递当前 Python 可导入的 pybind11 的 CMake 路径，避免硬编码安装位置。
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
