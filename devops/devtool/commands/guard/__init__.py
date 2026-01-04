from __future__ import annotations

from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import (
    cpp as cpp_ops,
    go as go_ops,
    python as python_ops,
    rust as rust_ops,
)

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("guard")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("guard", help="Run diagnostic guards (sanitizers/race checks)")
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "all"], nargs="?", default="all", help="Target to guard")
    parser.add_argument("--mode", help="Guard mode (for C++/Rust)", choices=["mem", "race", "miri", "heap", "all"], default=None)
    parser.add_argument("--filter", help="Filter expression", dest="filter_expr")
    parser.add_argument("--file", help="File pattern")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    if args.target in ("python", "all"):
        python_ops.guard(env, file_pattern=args.file, filter_expr=args.filter_expr)
    if args.target in ("go", "all"):
        go_ops.guard(env)
    if args.target in ("cpp", "all"):
        cpp_ops.guard(env, mode=args.mode)
    if args.target in ("rust", "all"):
        rust_ops.guard(env, mode=args.mode)
