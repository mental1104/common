from __future__ import annotations

import argparse
import importlib
import pkgutil
import sys
from typing import Iterable

from devtool.commands import CONFIGURATORS


def _import_command_modules() -> None:
    pkg = importlib.import_module("devtool.commands")
    prefix = pkg.__name__ + "."
    for module in pkgutil.walk_packages(pkg.__path__, prefix):  # type: ignore[attr-defined]
        name = module.name
        if name.split(".")[-1].startswith("_"):
            continue
        importlib.import_module(name)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Developer utilities entrypoint.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    _import_command_modules()
    for name, configurator in CONFIGURATORS.items():
        configurator(subparsers)
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    runner = getattr(args, "_runner", None)
    if runner is None:
        parser.print_help()
        return 1
    return int(runner(args) or 0)


if __name__ == "__main__":
    sys.exit(main())
