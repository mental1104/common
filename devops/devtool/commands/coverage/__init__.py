from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import cpp as cpp_ops, export as export_ops, go as go_ops, python as python_ops, rust as rust_ops


@register("coverage")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("coverage", help="Generate coverage reports")
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "all"], nargs="?", default="all", help="Target to cover")
    parser.add_argument("--filter", help="Filter expression", dest="filter_expr")
    parser.add_argument("--file", help="File pattern")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose test output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    target = args.target
    if target in ("python", "all"):
        export_ops.build_export_cpp(env)
        python_ops.coverage(env, file_pattern=args.file, filter_expr=args.filter_expr)
    if target in ("go", "all"):
        go_ops.coverage(env)
    if target in ("cpp", "all"):
        cpp_ops.test(env, file_pattern=args.file, filter_expr=args.filter_expr)
        cpp_ops.coverage(env)
    if target in ("rust", "all"):
        rust_ops.coverage(env)
