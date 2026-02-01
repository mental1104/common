from __future__ import annotations

import platform
import shutil
import subprocess
import urllib.parse
from typing import Mapping

from devtool.commands import register
from devtool.commands.common import base_env, run

DEFAULT_LOCAL_IMAGE = "mental1104_dev:latest"
DEFAULT_REMOTE_IMAGE = "mental1104/dev:latest"
DEFAULT_REGISTRY = "docker.io"
_PROXY_BUILD_ARGS = (
    "HTTP_PROXY",
    "HTTPS_PROXY",
    "NO_PROXY",
    "ALL_PROXY",
    "http_proxy",
    "https_proxy",
    "no_proxy",
    "all_proxy",
)
_EXTRA_BUILD_ARGS = ("NUGET_SOURCE",)
_PROXY_ENV_KEYS = ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy")


def _proxy_hosts(env: Mapping[str, str]) -> set[str]:
    hosts: set[str] = set()
    for key in _PROXY_ENV_KEYS:
        raw = env.get(key)
        if not raw:
            continue
        val = str(raw).strip()
        if not val:
            continue
        if "://" not in val:
            val = f"http://{val}"
        parsed = urllib.parse.urlparse(val)
        if parsed.hostname:
            hosts.add(parsed.hostname.lower())
    return hosts


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
    env["DOCKER_BUILDKIT"] = "1"
    _ensure_linux("build-docker")
    docker_bin = _ensure_docker()
    _require_envs(env, {"SSH_PRIVATE_KEY": "your_ssh_password"}, "build-docker")
    local_image = env.get("DOCKER_BUILD_IMAGE", DEFAULT_LOCAL_IMAGE)
    remote_image = env.get("DOCKER_PUSH_IMAGE", DEFAULT_REMOTE_IMAGE)
    cmd = [docker_bin, "build", "-t", local_image, ".", "--build-arg", "SSH_PRIVATE_KEY"]
    base_image = env.get("DOCKER_BUILD_BASE_IMAGE") or env.get("DOCKER_BASE_IMAGE")
    if base_image:
        cmd += ["--build-arg", f"BASE_IMAGE={base_image}"]
    build_network = env.get("DOCKER_BUILD_NETWORK")
    proxy_hosts = _proxy_hosts(env)
    if not build_network and proxy_hosts.intersection({"127.0.0.1", "localhost"}):
        build_network = "host"
    if build_network:
        cmd += ["--network", build_network]
    add_hosts_raw = env.get("DOCKER_BUILD_ADD_HOST", "")
    if not add_hosts_raw and "host.docker.internal" in proxy_hosts:
        add_hosts_raw = "host.docker.internal:host-gateway"
    for entry in [h.strip() for h in add_hosts_raw.split(",") if h.strip()]:
        cmd += ["--add-host", entry]
    for key in _PROXY_BUILD_ARGS:
        if env.get(key):
            cmd += ["--build-arg", key]
    for key in _EXTRA_BUILD_ARGS:
        if env.get(key):
            cmd += ["--build-arg", key]
    run(cmd, env=env)
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
