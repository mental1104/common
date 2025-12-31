from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import cpp as cpp_ops, go as go_ops, python as python_ops, rust as rust_ops


@register("fmt")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("fmt", help="Format code")
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "all"], nargs="?", default="all", help="Target to format")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    if args.target in ("python", "all"):
        python_ops.fmt(env)
    if args.target in ("go", "all"):
        go_ops.fmt(env)
    if args.target in ("cpp", "all"):
        cpp_ops.fmt(env)
    if args.target in ("rust", "all"):
        rust_ops.fmt(env)
