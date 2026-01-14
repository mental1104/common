from __future__ import annotations

import platform
import shutil
import subprocess
from typing import Mapping

from devtool.commands import register
from devtool.commands.common import base_env, run

DEFAULT_LOCAL_IMAGE = "mental1104_dev:latest"
DEFAULT_REMOTE_IMAGE = "mental1104/dev:latest"
DEFAULT_REGISTRY = "docker.io"


def _ensure_linux(command: str) -> None:
    if platform.system().lower() != "linux":
        system = platform.system()
        raise SystemExit(f"[err] {command} 仅支持 Linux，当前系统为 {system}，已拒绝执行。")


def _ensure_docker() -> str:
    docker_bin = shutil.which("docker")
    if not docker_bin:
        raise SystemExit("[err] 未找到 docker，请先安装并确保 docker 在 PATH 中。")
    return docker_bin


def _require_envs(env: Mapping[str, str], required: Mapping[str, str], purpose: str) -> dict[str, str]:
    missing: list[tuple[str, str]] = []
    values: dict[str, str] = {}
    for key, hint in required.items():
        raw = env.get(key)
        if raw is None or str(raw).strip() == "":
            missing.append((key, hint))
        else:
            values[key] = str(raw).strip()
    if missing:
        keys = ", ".join(key for key, _ in missing)
        print(f"[err] {purpose} 缺少环境变量: {keys}")
        print("请先执行：")
        for key, hint in missing:
            print(f'export {key}="{hint}"')
        raise SystemExit(1)
    return values


def _docker_login(env: Mapping[str, str], docker_bin: str, registry: str) -> None:
    creds = _require_envs(
        env,
        {
            "DOCKER_USERNAME": "your_docker_id",
            "DOCKER_PASSWORD": "your_token_or_password",
        },
        "push-docker",
    )
    print(f">> docker login {registry}")
    login_env = {k: str(v) for k, v in env.items()}
    subprocess.run(
        [docker_bin, "login", registry, "--username", creds["DOCKER_USERNAME"], "--password-stdin"],
        input=creds["DOCKER_PASSWORD"],
        text=True,
        check=True,
        env=login_env,
    )


@register("build-docker")
def configure_build(subparsers):
    parser = subparsers.add_parser("build-docker", help="Build docker image (Linux only)")
    parser.set_defaults(_runner=run_build)
    return run_build


def run_build(_args) -> None:
    env = base_env()
    _ensure_linux("build-docker")
    docker_bin = _ensure_docker()
    _require_envs(env, {"SSH_PRIVATE_KEY": "your_ssh_password"}, "build-docker")
    local_image = env.get("DOCKER_BUILD_IMAGE", DEFAULT_LOCAL_IMAGE)
    remote_image = env.get("DOCKER_PUSH_IMAGE", DEFAULT_REMOTE_IMAGE)
    run([docker_bin, "build", "-t", local_image, ".", "--build-arg", "SSH_PRIVATE_KEY"], env=env)
    run([docker_bin, "tag", local_image, remote_image], env=env)
    print(f"[ok] build-docker 完成: {local_image} -> {remote_image}")


@register("push-docker")
def configure_push(subparsers):
    parser = subparsers.add_parser("push-docker", help="Login to docker.io and push image (Linux only)")
    parser.set_defaults(_runner=run_push)
    return run_push


def run_push(_args) -> None:
    env = base_env()
    _ensure_linux("push-docker")
    docker_bin = _ensure_docker()
    registry = env.get("DOCKER_REGISTRY", DEFAULT_REGISTRY)
    _docker_login(env, docker_bin, registry)
    remote_image = env.get("DOCKER_PUSH_IMAGE", DEFAULT_REMOTE_IMAGE)
    run([docker_bin, "push", remote_image], env=env)
    print(f"[ok] push-docker 完成: {remote_image}")
