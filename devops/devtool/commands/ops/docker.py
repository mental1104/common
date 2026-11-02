from __future__ import annotations

import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Mapping, Optional

from devtool.commands.common import ROOT, run


def visitor_root(env: Mapping[str, str]) -> Path:
    root = Path(env.get("VISITOR_ROOT") or ROOT.parent / "visitor").expanduser().resolve()
    if not (root / "tools" / "development" / "cli.py").is_file():
        raise SystemExit(
            f"[err] 开发镜像已迁往 mental1104/visitor。请检出 visitor 并设置 VISITOR_ROOT：{root}"
        )
    return root


def run_visitor(command: str, env: Mapping[str, str]) -> None:
    root = visitor_root(env)
    forwarded = dict(env)
    forwarded["COMMON_ROOT"] = str(ROOT)
    run([sys.executable, str(root / "tools" / "development" / "cli.py"),
         "--common-root", str(ROOT), command], env=forwarded, cwd=root)


def _compose_cmd_ok(cmd: list[str]) -> bool:
    if not cmd or not shutil.which(cmd[0]):
        return False
    try:
        res = subprocess.run([*cmd, "version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return res.returncode == 0
    except Exception:
        return False


def _detect_compose(env: Mapping[str, str]) -> Optional[list[str]]:
    """Return compose command argv if available; prefer docker compose."""
    user = env.get("COMPOSE_BIN")
    if user:
        cmd = shlex.split(user)
        if _compose_cmd_ok(cmd):
            return cmd
    docker_bin = shutil.which("docker")
    if docker_bin and _compose_cmd_ok([docker_bin, "compose"]):
        return [docker_bin, "compose"]
    if _compose_cmd_ok(["docker-compose"]):
        return ["docker-compose"]
    return None


def run_docker(env: Mapping[str, str]) -> None:
    run_visitor("run-docker", env)


def setup_docker(env: Mapping[str, str]) -> None:
    run_visitor("setup-docker", env)


def clean_docker(env: Mapping[str, str]) -> None:
    run_visitor("clean-docker", env)
