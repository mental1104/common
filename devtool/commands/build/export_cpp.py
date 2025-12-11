from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import export as export_ops


@register("build-export-cpp")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("build-export-cpp", help="Build export/cpp bridge")
    parser.add_argument("--config", default="Debug", help="CMake build type")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs, cpp_build_type=args.config)
    export_ops.build_export_cpp(env)
