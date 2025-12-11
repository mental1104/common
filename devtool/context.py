from __future__ import annotations

import os
import platform
import shlex
import subprocess
from pathlib import Path
from typing import Mapping, Sequence
from typing import Mapping


ROOT = Path(__file__).resolve().parents[1]


_DEBUG_LEVELS = {"DEBUG", "TRACE"}


def _should_echo_cmd(env: Mapping[str, str] | None) -> bool:
    """Only echo commands when caller opts into debug/verbose output."""
    def _read_level(source: Mapping[str, str]) -> str:
        for key in ("DEV_LOG_LEVEL", "MENTAL1104_LOG_LEVEL"):
            if key in source:
                return str(source[key]).strip().upper()
        if source.get("VERBOSE") in ("1", 1, True):
            return "DEBUG"
        return ""

    level = ""
    if env:
        level = _read_level(env)
    if not level:
        level = _read_level(os.environ)
    return level in _DEBUG_LEVELS


def sh(cmd: str | Sequence[str], cwd: Path | None = None, env: Mapping[str, str] | None = None) -> None:
    """Run a command with unified logging (no implicit shell)."""
    workdir = cwd or ROOT
    if isinstance(cmd, str):
        args = shlex.split(cmd)
        display = cmd
    else:
        args = list(cmd)
        display = " ".join(shlex.quote(str(x)) for x in args)
    if _should_echo_cmd(env):
        print(f"[dev][DEBUG] ({workdir})$ {display}")
    if env:
        run_env = {k: str(v) for k, v in env.items()}
    else:
        run_env = os.environ.copy()
    subprocess.run(args, cwd=str(workdir), check=True, env=run_env)


def is_windows() -> bool:
    return platform.system() == "Windows"
