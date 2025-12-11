from __future__ import annotations

import argparse
import importlib
import pkgutil
import shlex
import subprocess
import sys
import traceback
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devtool.context import ROOT as CTX_ROOT

from devtool.commands import CONFIGURATORS


_SKIP_FILES = {
    (CTX_ROOT / "devtool" / "context.py").resolve(),
    (CTX_ROOT / "devtool" / "commands" / "common.py").resolve(),
}


def _format_cmd(cmd: object) -> str:
    if cmd is None:
        return ""
    if isinstance(cmd, (list, tuple)):
        return " ".join(shlex.quote(str(x)) for x in cmd)
    return str(cmd)


def _subprocess_location(tb) -> str:
    frames = traceback.extract_tb(tb) if tb else []
    chain: list[str] = []
    for frame in frames:
        path = Path(frame.filename).resolve()
        if path in _SKIP_FILES:
            continue
        try:
            rel = path.relative_to(CTX_ROOT)
        except ValueError:
            continue
        chain.append(f"{rel}:{frame.lineno}({frame.name})")
    if not chain and frames:
        frame = frames[-1]
        chain.append(f"{frame.filename}:{frame.lineno}({frame.name})")
    return " -> ".join(chain)


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
    try:
        runner = getattr(args, "_runner", None)
        if runner is None:
            parser.print_help()
            return 1
        return int(runner(args) or 0)
    except subprocess.CalledProcessError as exc:
        location = _subprocess_location(exc.__traceback__)
        cmd_display = _format_cmd(getattr(exc, "cmd", None))
        print(f"[dev] 子进程执行失败，退出码 {exc.returncode}")
        if location:
            print(f"[dev] 调用链：{location}")
        if cmd_display:
            print(f"[dev] 命令：{cmd_display}")
        return exc.returncode


if __name__ == "__main__":
    sys.exit(main())
