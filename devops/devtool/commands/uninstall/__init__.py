from __future__ import annotations

from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import (
    cpp as cpp_ops,
    dotnet as dotnet_ops,
    go as go_ops,
    java as java_ops,
    python as python_ops,
    rust as rust_ops,
)

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("uninstall")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("uninstall", help="Uninstall installed artifacts")
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "dotnet", "java", "all"], nargs="?", default="all", help="Target to uninstall")
    parser.add_argument("--prefix", help="Install prefix for C++ (and similar)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs, prefix=args.prefix)
    if args.target in ("python", "all"):
        python_ops.uninstall(env)
    if args.target in ("go", "all"):
        go_ops.uninstall(env)
    if args.target in ("cpp", "all"):
        cpp_ops.uninstall(env)
    if args.target in ("rust", "all"):
        rust_ops.uninstall(env)
    if args.target in ("dotnet", "all"):
        dotnet_ops.uninstall(env)
    if args.target in ("java", "all"):
        java_ops.uninstall(env)
