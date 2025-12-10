from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import rust as rust_ops


@register("setup-rust")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("setup-rust", help="Setup Rust toolchain/deps")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    rust_ops.setup(env)
