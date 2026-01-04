from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import cpp as cpp_ops, go as go_ops, python as python_ops, rust as rust_ops


@register("build")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("build", help="Build project components")
    parser.add_argument("target", choices=["python", "cpp", "go", "rust", "all"], nargs="?", default="all", help="Target to build")
    parser.add_argument("--config", default="Debug", help="CMake build type for C++ targets")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output for tools")
    parser.set_defaults(_runner=run)
    return run


def _build_cpp(env):
    cpp_ops.git_submodules(env)
    cpp_ops.build_submodules(env)
    cpp_ops.configure(env)
    cpp_ops.build(env)


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs, cpp_build_type=args.config)
    target = args.target
    if target in ("python", "all"):
        python_ops.build(env)
    if target in ("go", "all"):
        go_ops.setup(env)
        go_ops.build(env)
    if target in ("cpp", "all"):
        _build_cpp(env)
    if target in ("rust", "all"):
        rust_ops.setup(env)
        rust_ops.build(env)
