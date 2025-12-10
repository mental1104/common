from __future__ import annotations

import os
import platform
import shlex
import subprocess
from pathlib import Path
from typing import Mapping, Sequence
from typing import Mapping


ROOT = Path(__file__).resolve().parents[1]


def sh(cmd: str | Sequence[str], cwd: Path | None = None, env: Mapping[str, str] | None = None) -> None:
    """Run a command with unified logging (no implicit shell)."""
    workdir = cwd or ROOT
    if isinstance(cmd, str):
        args = shlex.split(cmd)
        display = cmd
    else:
        args = list(cmd)
        display = " ".join(shlex.quote(str(x)) for x in args)
    print(f"[dev] ({workdir})$ {display}")
    if env:
        run_env = {k: str(v) for k, v in env.items()}
    else:
        run_env = os.environ.copy()
    subprocess.run(args, cwd=str(workdir), check=True, env=run_env)


def is_windows() -> bool:
    return platform.system() == "Windows"
