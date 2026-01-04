from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import (
    cpp as cpp_ops,
    export as export_ops,
    go as go_ops,
    python as python_ops,
    rust as rust_ops,
)
from devtool.context import sh

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("bench")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("bench", help="Run benchmarks")
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "all"], nargs="?", default="all", help="Target to benchmark")
    parser.add_argument("--filter", help="Filter expression", dest="filter_expr")
    parser.add_argument("--file", help="File pattern")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    ran = False
    if args.target in ("python", "all"):
        export_ops.build_export_cpp(env)
        python_ops.bench(env, file_pattern=args.file, filter_expr=args.filter_expr)
        ran = True
    if args.target in ("go", "all"):
        go_ops.bench(env, file_pattern=args.file, filter_expr=args.filter_expr)
        ran = True
    if args.target in ("cpp", "all"):
        cpp_ops.bench(env, file_pattern=args.file, filter_expr=args.filter_expr)
        ran = True
    if args.target in ("rust", "all"):
        rust_ops.bench(env, file_pattern=args.file, filter_expr=args.filter_expr)
        ran = True
    if ran:
        root = Path(env["BENCH_ARTIFACT_ROOT"])
        output = root / "index.html"
        root.mkdir(parents=True, exist_ok=True)
        sh(
            f'{env.get("PYTHON", "python3")} python/tools/assemble_bench_gallery.py --root "{root}" --output "{output}"',
            env=env,
        )
