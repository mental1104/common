from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.context import sh

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("bench-report")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("bench-report", help="Assemble benchmark gallery without rerunning benches")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    root = Path(env["BENCH_ARTIFACT_ROOT"])
    output = root / "index.html"
    root.mkdir(parents=True, exist_ok=True)
    sh(
        f'{env.get("PYTHON", "python3")} python/tools/assemble_bench_gallery.py --root "{root}" --output "{output}"',
        env=env,
    )
    print(f"[bench] 图库：{output}")
