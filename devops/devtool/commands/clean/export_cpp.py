from __future__ import annotations

import shutil
from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import EXPORT_CPP_BUILD_DIR, base_env

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("clean-export-cpp")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("clean-export-cpp", help="Clean export/cpp build directory")
    parser.set_defaults(_runner=run)
    return run


def run(_args):
    base_env()
    shutil.rmtree(EXPORT_CPP_BUILD_DIR, ignore_errors=True)
    print("[ok] cleaned export/cpp build")
