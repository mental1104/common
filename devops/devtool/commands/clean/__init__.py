from __future__ import annotations

from argparse import ArgumentParser
from pathlib import Path

from devtool.commands import register
from devtool.commands.common import ROOT, base_env
from devtool.commands.ops import cpp as cpp_ops, go as go_ops, python as python_ops, rust as rust_ops


def _clean_env_files() -> None:
    env_stamp = ROOT / ".env.active"
    if env_stamp.exists():
        env_stamp.unlink()
        print(f"[info] 已移除环境标记文件: {env_stamp}")


def _add_common_args(parser: ArgumentParser) -> None:
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "all"], nargs="?", default="all", help="Target to clean")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")


@register("clean")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("clean", help="Clean build artifacts (keep submodule builds intact)")
    _add_common_args(parser)
    parser.set_defaults(clean_submodules=False, _runner=run)
    return run


@register("clean-all")
def configure_all(subparsers: ArgumentParser):
    parser = subparsers.add_parser("clean-all", help="Clean build artifacts (full, includes submodules)")
    _add_common_args(parser)
    parser.set_defaults(clean_submodules=True, _runner=run)
    return run


def run(args, *, clean_submodules: bool | None = None):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    clean_submods = clean_submodules
    if clean_submods is None:
        clean_submods = getattr(args, "clean_submodules", True)
    if args.target in ("python", "all"):
        python_ops.clean(env)
    if args.target in ("go", "all"):
        go_ops.clean(env)
    if args.target in ("cpp", "all"):
        cpp_ops.clean(env, clean_submodules=clean_submods)
    if args.target in ("rust", "all"):
        rust_ops.clean(env)
    if args.target == "all":
        _clean_env_files()
