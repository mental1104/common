from __future__ import annotations

import os
from dataclasses import dataclass, field
from threading import Lock
from typing import Any, Dict, Mapping, Optional

import redis

from .config import RedisConnParams, RedisMode, redis_params_from_env
from .factory import create_redis_client


@dataclass(frozen=True)
class RedisConfig:
    name: str
    url: Optional[str] = None
    params: Optional[RedisConnParams] = None
    mode: RedisMode = RedisMode.STANDALONE
    options: Mapping[str, Any] = field(default_factory=dict)


def _resolve_mode(mode: RedisMode | str | None, options: Mapping[str, Any]) -> RedisMode:
    raw = mode or options.get("mode") or os.environ.get("REDIS_MODE", "standalone")
    raw = str(raw).strip().lower()
    if raw in ("standalone", "single"):
        return RedisMode.STANDALONE
    if raw == "cluster":
        return RedisMode.CLUSTER
    if raw == "sentinel":
        return RedisMode.SENTINEL
    raise ValueError(f"unsupported redis mode: {raw}")


class RedisRegistry:
    def __init__(self) -> None:
        self._configs: Dict[str, RedisConfig] = {}
        self._clients: Dict[str, redis.Redis] = {}
        self._lock = Lock()

    def register(
        self,
        *,
        name: str = "default",
        url: Optional[str] = None,
        params: Optional[RedisConnParams] = None,
        mode: Optional[RedisMode | str] = None,
        options: Optional[Mapping[str, Any]] = None,
        allow_overwrite: bool = False,
    ) -> RedisConfig:
        if not url and params is None:
            params = redis_params_from_env()
        if not allow_overwrite and name in self._configs:
            raise ValueError(f"redis '{name}' already registered")
        merged_options = dict(options or {})
        resolved_mode = _resolve_mode(mode, merged_options)
        if "mode" in merged_options:
            merged_options.pop("mode")
        config = RedisConfig(
            name=name,
            url=url,
            params=params,
            mode=resolved_mode,
            options=merged_options,
        )
        self._configs[name] = config
        return config

    def get_config(self, name: str = "default") -> RedisConfig:
        try:
            return self._configs[name]
        except KeyError as exc:
            raise KeyError(f"redis '{name}' is not registered") from exc

    def get_client(self, name: str = "default") -> redis.Redis:
        if name in self._clients:
            return self._clients[name]
        with self._lock:
            if name in self._clients:
                return self._clients[name]
            cfg = self.get_config(name)
            client = create_redis_client(
                params=cfg.params,
                url=cfg.url,
                mode=cfg.mode,
                options=cfg.options,
            )
            self._clients[name] = client
            return client

    def close(self, name: str = "default") -> None:
        client = self._clients.pop(name, None)
        if client is None:
            return
        try:
            client.close()
        except Exception:
            pass

    def close_all(self) -> None:
        names = list(self._clients.keys())
        for name in names:
            self.close(name)


DEFAULT_REDIS_REGISTRY = RedisRegistry()


def register_redis(
    *,
    name: str = "default",
    url: Optional[str] = None,
    params: Optional[RedisConnParams] = None,
    mode: Optional[RedisMode | str] = None,
    options: Optional[Mapping[str, Any]] = None,
    allow_overwrite: bool = False,
) -> RedisConfig:
    return DEFAULT_REDIS_REGISTRY.register(
        name=name,
        url=url,
        params=params,
        mode=mode,
        options=options,
        allow_overwrite=allow_overwrite,
    )


def get_redis_client(name: str = "default") -> redis.Redis:
    return DEFAULT_REDIS_REGISTRY.get_client(name)
