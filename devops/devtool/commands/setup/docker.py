from __future__ import annotations

from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import docker

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("setup-docker")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("setup-docker", help="Start docker compose stacks under devops/images/")
    parser.set_defaults(_runner=run)
    return run


def run(_args):
    env = base_env()
    docker.setup_docker(env)
