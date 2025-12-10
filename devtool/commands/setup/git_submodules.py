from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import cpp as cpp_ops


@register("git-submodules")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("git-submodules", help="Fetch/update git submodules")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose)
    cpp_ops.git_submodules(env)
