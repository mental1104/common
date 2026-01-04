from __future__ import annotations

from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import cpp as cpp_ops

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("setup-cpp")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("setup-cpp", help="Prepare C++ build (submodules + configure)")
    parser.add_argument("--config", default="Debug", help="CMake build type")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs, cpp_build_type=args.config)
    cpp_ops.prepare_submodules(env, skip_when_ready=True)
    cpp_ops.configure(env)
