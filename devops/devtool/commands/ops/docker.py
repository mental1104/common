from __future__ import annotations

import re
import shlex
import shutil
import subprocess
from typing import TYPE_CHECKING, Iterable, Mapping, Optional

from devtool.commands.common import ROOT, run

if TYPE_CHECKING:
    from pathlib import Path

_CONTAINER_NAME_RE = re.compile(r"container_name:\s*(.+)", re.IGNORECASE)


def _running_container_names() -> set[str]:
    docker_bin = shutil.which("docker")
    if not docker_bin:
        return set()
    try:
        res = subprocess.run(
            [docker_bin, "ps", "--format", "{{.Names}}"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True
        )
    except Exception:
        return set()
    names: set[str] = set()
    for line in res.stdout.splitlines():
        name = line.strip()
        if name:
            names.add(name)
    return names


def _compose_container_names(compose_file: Path) -> set[str]:
    try:
        content = compose_file.read_text()
    except FileNotFoundError:
        return set()
    names: set[str] = set()
    for raw in content.splitlines():
        m = _CONTAINER_NAME_RE.search(raw)
        if m:
            name = m.group(1).strip().strip('"').strip("'")
            if name:
                names.add(name)
    return names


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
    compose_cmd = _detect_compose(env)
    if compose_cmd is None:
        print("[warn] 未检测到 docker compose（docker compose 或 docker-compose），跳过 run-docker")
        return
    compose_file = ROOT / env.get("COMPOSE_FILE_NAME", "docker-compose.yaml")
    if not compose_file.exists():
        print(f"[warn] 未找到 {compose_file}，跳过 run-docker")
        return
    env_src, _ = _env_files()
    env_file_opt: Iterable[str] = ["--env-file", str(env_src)] if env_src.exists() else []
    cmd_base = [*compose_cmd, "-f", str(compose_file), *env_file_opt]
    try:
        res = subprocess.run(
            [*cmd_base, "ps", "-q"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            env={k: str(v) for k, v in env.items()},
        )
    except Exception:
        res = None
    running = bool(res and res.returncode == 0 and res.stdout.strip())
    if running:
        print(">> DOWN (root compose)")
        run([*cmd_base, "down"], env=env)
    else:
        print("[skip] compose 未在运行，跳过 down")
    print(">> UP (root compose)")
    run([*cmd_base, "up", "-d"], env=env)


def _env_files() -> tuple[Path, Path]:
    env_src = ROOT / ".env"
    env_stamp = ROOT / ".env.active"
    return env_src, env_stamp


def _compose_dirs(env: Mapping[str, str]) -> list[Path]:
    file_name = env.get("COMPOSE_FILE_NAME", "docker-compose.yaml")
    images_dir = ROOT / "devops" / "images"
    if not images_dir.exists():
        return []
    dirs = {p.parent for p in images_dir.rglob(file_name)}
    return sorted(dirs)


def setup_docker(env: Mapping[str, str]) -> None:
    compose_cmd = _detect_compose(env)
    if compose_cmd is None:
        print("[warn] 未检测到 docker compose（docker-compose 或 docker compose），跳过 setup-docker")
        return
    env_src, env_stamp = _env_files()
    compose_dirs = _compose_dirs(env)
    if not compose_dirs:
        print("[warn] devops/images/ 未找到 docker-compose.yaml")
        return
    env_file_opt: Iterable[str] = ["--env-file", str(env_src)] if env_src.exists() else []
    cmd_base = compose_cmd
    running = _running_container_names()
    for d in compose_dirs:
        compose_file = d / env.get("COMPOSE_FILE_NAME", "docker-compose.yaml")
        container_names = _compose_container_names(compose_file)
        if container_names and container_names.issubset(running):
            print(f"[skip] {d} 已在运行，跳过")
            continue
        print(f">> UP {d}")
        project_name = d.name
        try:
            run(
                [
                    *cmd_base,
                    "--project-directory",
                    str(ROOT),
                    "--project-name",
                    project_name,
                    *env_file_opt,
                    "-f",
                    str(compose_file),
                    "up",
                    "-d",
                    "--no-recreate",
                ],
                env=env,
            )
            running.update(container_names)
        except Exception:
            print(f"[warn] {d} 启动失败（可能是已有同名容器占用，需先手动 docker rm -f <name>；已忽略）")
            continue
    env_stamp.touch(exist_ok=True)
    print("[ok] setup-docker 完成（出错已忽略）")


def clean_docker(env: Mapping[str, str]) -> None:
    compose_cmd = _detect_compose(env)
    if compose_cmd is None:
        print("[warn] 未检测到 docker compose（docker-compose 或 docker compose），跳过 clean-docker")
        return
    env_src, env_stamp = _env_files()
    compose_dirs = _compose_dirs(env)
    if not compose_dirs:
        print("[warn] devops/images/ 未找到 docker-compose.yaml")
    env_file_opt: Iterable[str] = ["--env-file", str(env_src)] if env_src.exists() else []
    cmd_base = compose_cmd
    for d in compose_dirs:
        print(f">> DOWN {d}")
        project_name = d.name
        try:
            run(
                [
                    *cmd_base,
                    "--project-directory",
                    str(ROOT),
                    "--project-name",
                    project_name,
                    *env_file_opt,
                    "-f",
                    str(d / env.get("COMPOSE_FILE_NAME", "docker-compose.yaml")),
                    "down",
                    "--remove-orphans",
                ],
                env=env,
            )
        except Exception:
            print(f"[warn] {d} 关闭失败（已忽略）")
            continue
    env_stamp.unlink(missing_ok=True)
    print("[ok] clean-docker 完成（出错已忽略）")
