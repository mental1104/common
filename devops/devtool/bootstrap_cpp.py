"""从 CMake 自动准备 C++ 源码依赖与独立 SDK 构建树，不递归配置主工程。"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

from devtool.commands.common import ROOT, base_env
from devtool.commands.ops import cpp


@dataclass(frozen=True)
class _Options:
    """CMake 传入的具体构建配置；路径与标准都对应当前构建树。"""

    dependency_root: Path
    standard: int
    jobs: int
    build_type: str
    cmake: str
    redis_enabled: bool
    fetch_sources: bool
    cmake_options: tuple[str, ...]


class _Bootstrap:
    """复用现有 submodule 流程，源码共享，SDK 二进制按构建目录隔离。"""

    def __init__(self, options: _Options) -> None:
        """保存不可变配置。

        Args:
            options: 当前构建树的路径、编译标准及开关。
        """
        self._options = options

    @staticmethod
    def _populated(path: Path) -> bool:
        """检查源码目录是否已检出，不把 Git 标记或构建产物算成源码。

        Args:
            path: 待读取的子模块源码路径。
        Returns:
            存在源码条目时为真，不修改源码目录。
        """
        return path.is_dir() and any(
            entry.name not in {".git", ".gitmodules", "build"}
            for entry in path.iterdir()
        )

    def _prepare_sources(self, env: Mapping[str, str]) -> None:
        """仅拉取缺失源码，沿用仓库 Boost 稀疏检出和子模块版本清单。

        Args:
            env: 只读的子进程配置；显式跳过列表继续生效。
        """
        # 与 devtool 共用同一份 manifest 和 skip 规则，不维护第二套依赖注册表。
        skipped = cpp._skip_list(env)
        missing = {
            relative
            for relative in cpp._gitmodule_paths()
            if relative not in skipped and not self._populated(ROOT / relative)
        }
        boost = "cpp/lib/boost"
        if boost not in skipped and any(
            not self._populated(ROOT / boost / module)
            for module in cpp._load_boost_required_modules()
        ):
            missing.add(boost)
        if not missing:
            return
        if not self._options.fetch_sources:
            raise RuntimeError(
                "C++ sources are missing; enable AUTO_FETCH_SUBMODULES "
                "or initialize them before configuring"
            )
        # 已有源码不参与 update，避免因另一个缺失模块触发对已有检出的回退覆盖。
        fetch_env = dict(env)
        fetch_env["SKIP_SUBMODULES"] = " ".join(
            sorted(skipped | (set(cpp._gitmodule_paths()) - missing))
        )
        cpp.git_submodules(fetch_env)
        if any(not self._populated(ROOT / relative) for relative in missing):
            raise RuntimeError(
                "C++ submodule initialization did not provide all required sources"
            )

    def run(self) -> None:
        """准备缺失源码，并仅构建主库实际需要的 Redis SDK。"""
        options = self._options
        env = base_env(jobs=options.jobs, cpp_build_type=options.build_type)
        env["CMAKE"] = options.cmake
        env["JOBS"] = str(options.jobs)
        env["CXX_STD"] = str(options.standard)
        env["REDISPP_CXX_STD"] = str(options.standard)
        self._prepare_sources(env)
        if options.redis_enabled:
            # cJSON 由主工程的 PIC fallback target 编译，不生成另一份源码树二进制。
            cpp.build_submodules(
                env,
                strict=True,
                only={"cpp/lib/hiredis", "cpp/lib/redis-plus-plus"},
                build_root=options.dependency_root,
                cmake_options=options.cmake_options,
            )


def main() -> None:
    """解析 CMake 参数并运行准备流程；任何失败均以非零状态传播给配置阶段。"""
    parser = argparse.ArgumentParser(
        description="Prepare C++ dependencies for one CMake build tree"
    )
    parser.add_argument("--dependency-root", type=Path, required=True)
    parser.add_argument("--standard", type=int, required=True)
    parser.add_argument("--jobs", type=int, required=True)
    parser.add_argument("--build-type", required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--redis-enabled", action="store_true")
    parser.add_argument("--fetch-sources", action="store_true")
    parser.add_argument("--cmake-arg", action="append", default=[])
    args = parser.parse_args()
    options = _Options(
        dependency_root=args.dependency_root,
        standard=args.standard,
        jobs=args.jobs,
        build_type=args.build_type,
        cmake=args.cmake,
        redis_enabled=args.redis_enabled,
        fetch_sources=args.fetch_sources,
        cmake_options=tuple(args.cmake_arg),
    )
    _Bootstrap(options).run()


if __name__ == "__main__":
    main()
