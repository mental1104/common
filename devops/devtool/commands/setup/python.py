from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import python as python_ops


@register("setup-python")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("setup-python", help="Setup Python venv and build wheel")
    parser.add_argument("--jobs", type=int, help="Override parallelism hint for downstream tools")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose test/tool output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    python_ops.setup(env)
