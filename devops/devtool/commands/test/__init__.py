from __future__ import annotations

from argparse import REMAINDER, ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import (
    cpp as cpp_ops,
    dotnet as dotnet_ops,
    export as export_ops,
    go as go_ops,
    python as python_ops,
    rust as rust_ops,
)


@register("test")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("test", help="Run tests")
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "dotnet", "all"], nargs="?", default="all", help="Target to test")
    parser.add_argument("--filter", help="Filter expression (maps to FILTER env)", dest="filter_expr")
    parser.add_argument("--file", help="File pattern (maps to FILE env)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose test output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint for ctest/cmake")
    parser.add_argument("extra_args", nargs=REMAINDER, help="Extra args for pytest after --")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, test_verbose=getattr(args, "test_verbose", None), jobs=args.jobs)
    target = args.target
    extra_args = args.extra_args[1:] if args.extra_args and args.extra_args[0] == "--" else args.extra_args

    if target in ("python", "all"):
        export_ops.build_export_cpp(env)
        python_ops.test(env, pytest_args=extra_args or [], file_pattern=args.file, filter_expr=args.filter_expr)
    if target in ("go", "all"):
        go_ops.test(env, file_pattern=args.file, filter_expr=args.filter_expr)
    if target in ("cpp", "all"):
        cpp_ops.test(env, file_pattern=args.file, filter_expr=args.filter_expr)
    if target in ("rust", "all"):
        rust_ops.test(env, file_pattern=args.file, filter_expr=args.filter_expr)
    if target in ("dotnet", "all"):
        dotnet_ops.test(env, file_pattern=args.file, filter_expr=args.filter_expr)
