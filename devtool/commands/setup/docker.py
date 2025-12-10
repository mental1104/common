from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import docker


@register("setup-docker")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("setup-docker", help="Start docker compose stacks under images/")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env()
    docker.setup_docker(env)
