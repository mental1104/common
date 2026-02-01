from __future__ import annotations

from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import java as java_ops

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("setup-java")
def configure(subparsers: "ArgumentParser"):
    parser = subparsers.add_parser("setup-java", help="Setup Java tooling (JDK/Maven)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    java_ops.setup(env)
