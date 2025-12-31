from __future__ import annotations

from urllib.parse import urlparse


PIP_INDEX_URL = "https://mirrors.aliyun.com/pypi/simple"
PIP_TRUSTED_HOST = "mirrors.aliyun.com"


def _infer_host(index_url: str) -> str:
    return urlparse(index_url).hostname or ""


def apply_pip_mirror_env(env: dict[str, str]) -> None:
    """Populate pip mirror env defaults unless explicitly provided."""
    if env.get("PIP_MIRROR_OPTS"):
        return
    index_url = env.get("PIP_INDEX_URL", "").strip() or PIP_INDEX_URL
    env["PIP_INDEX_URL"] = index_url
    trusted_host = env.get("PIP_TRUSTED_HOST", "").strip()
    if not trusted_host:
        trusted_host = _infer_host(index_url) or PIP_TRUSTED_HOST
    if trusted_host:
        env["PIP_TRUSTED_HOST"] = trusted_host
    mirror_opts = f"-i {index_url}"
    if trusted_host:
        mirror_opts += f" --trusted-host {trusted_host}"
    env["PIP_MIRROR_OPTS"] = mirror_opts
