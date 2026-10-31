from __future__ import annotations

import os
import platform
import shutil
import subprocess
from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import CPP_BUILD_DIR, base_env
from devtool.commands.ops import cpp as cpp_ops

if TYPE_CHECKING:
    from argparse import ArgumentParser


class _CppDependencies:
    """在拉取子模块和配置工程前检查宿主机所需的 C++ 构建依赖。"""

    def __init__(self, env: dict[str, str]):
        """保存构建环境，以便使用与 CMake 相同的编译器和 PATH。"""
        self.env = env

    def _coverage_enabled(self) -> bool:
        """读取已有 CMake 缓存；首次配置时覆盖率默认启用。"""
        cache = CPP_BUILD_DIR / "CMakeCache.txt"
        values: dict[str, str] = {}
        if cache.exists():
            for line in cache.read_text(errors="replace").splitlines():
                if ":BOOL=" in line and not line.startswith(("#", "//")):
                    key, value = line.split(":BOOL=", 1)
                    values[key] = value.strip().upper()
        return any(values.get(key, "ON") not in {"OFF", "FALSE", "0", "NO"}
                   for key in ("ENABLE_COVERAGE", "COVERAGE"))

    def _has_numeric_library(self, header: str, statement: str, libraries: list[str]) -> bool:
        """编译并链接最小程序，检查指定数值库的头文件与链接库。"""
        compiler = self.env.get("CXX") or "c++"
        if not shutil.which(compiler, path=self.env.get("PATH")):
            return False
        source = f"#include <{header}>\nint main() {{ {statement} return 0; }}\n"
        try:
            result = subprocess.run(
                [compiler, "-x", "c++", "-", *libraries, "-o", os.devnull],
                input=source, text=True, capture_output=True, env=self.env, check=False,
            )
        except OSError:
            return False
        return result.returncode == 0

    def check(self) -> None:
        """汇总缺失项并给出当前平台的安装命令，失败时阻止配置。"""
        missing: list[str] = []
        packages: list[str] = []
        if not shutil.which(self.env["CMAKE"], path=self.env.get("PATH")):
            missing.append("cmake")
            packages.append("cmake")
        system = platform.system().lower()
        compiler = self.env.get("CXX") or "c++"
        has_compiler = bool(shutil.which(compiler, path=self.env.get("PATH")))
        if not has_compiler:
            missing.append("C++ compiler (g++)")
            packages.append("g++" if system == "linux" else "gcc")
        if system != "windows" and has_compiler:
            if not self._has_numeric_library("gmp.h", "mpz_t x; mpz_init(x); mpz_clear(x);", ["-lgmp"]):
                missing.append("GMP development headers/library (libgmp-dev)")
                packages.append("libgmp-dev" if system == "linux" else "gmp")
            if not self._has_numeric_library("mpfr.h", "mpfr_t x; mpfr_init2(x, 32); mpfr_clear(x);", ["-lmpfr", "-lgmp"]):
                missing.append("MPFR development headers/library (libmpfr-dev)")
                packages.append("libmpfr-dev" if system == "linux" else "mpfr")
        if system != "windows" and self._coverage_enabled():
            coverage_missing = False
            for executable in ("lcov", "genhtml"):
                if not shutil.which(executable, path=self.env.get("PATH")):
                    missing.append(executable)
                    coverage_missing = True
            if coverage_missing:
                packages.append("lcov")
        if not missing:
            return
        print("[error] Missing C++ build dependencies: " + ", ".join(missing))
        if system == "linux":
            print("[hint] Ubuntu/Debian: sudo apt update && sudo apt install " + " ".join(packages))
        elif system == "darwin":
            print("[hint] macOS: brew install " + " ".join(packages))
        raise SystemExit(1)


@register("setup-cpp")
def configure(subparsers: ArgumentParser):
    """注册 C++ 环境准备命令及其构建选项。"""
    parser = subparsers.add_parser("setup-cpp", help="Prepare C++ build (submodules + configure)")
    parser.add_argument("--config", default="Debug", help="CMake build type")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    """先校验宿主机依赖，再准备子模块并配置 CMake。"""
    env = base_env(verbose=args.verbose, jobs=args.jobs, cpp_build_type=args.config)
    _CppDependencies(env).check()
    cpp_ops.prepare_submodules(env, skip_when_ready=True)
    cpp_ops.configure(env)
