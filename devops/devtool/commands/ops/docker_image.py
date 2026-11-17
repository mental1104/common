from __future__ import annotations

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops.docker import run_visitor


@register("build-docker")
def configure_build(subparsers):
    parser = subparsers.add_parser("build-docker", help="Build development image in the visitor checkout")
    parser.set_defaults(_runner=run_build)
    return run_build


def run_build(_args) -> None:
    run_visitor("build-docker", base_env())


@register("push-docker")
def configure_push(subparsers):
    parser = subparsers.add_parser("push-docker", help="Publish development image through visitor")
    parser.set_defaults(_runner=run_push)
    return run_push


def run_push(_args) -> None:
    run_visitor("push-docker", base_env())
