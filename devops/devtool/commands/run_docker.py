from __future__ import annotations

from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import docker

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("run-docker")
def configure(subparsers: "ArgumentParser"):
    parser = subparsers.add_parser("run-docker", help="Restart root docker compose stack")
    parser.set_defaults(_runner=run)
    return run


def run(_args):
    env = base_env()
    docker.run_docker(env)
