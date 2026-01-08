from __future__ import annotations

import os
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Mapping, Optional


class RedisMode(str, Enum):
    STANDALONE = "standalone"
    CLUSTER = "cluster"
    SENTINEL = "sentinel"


@dataclass(frozen=True)
class RedisConnParams:
    host: str
    port: int = 6379
    password: Optional[str] = None
    db: int = 0
    username: Optional[str] = None
    options: Mapping[str, Any] = field(default_factory=dict)


def _parse_bool(raw: Optional[str], default: bool) -> bool:
    if raw is None:
        return default
    return str(raw).strip().lower() in ("1", "true", "yes", "on")


def redis_params_from_env(prefix: str = "") -> RedisConnParams:
    env = os.environ
    host = env.get(f"{prefix}REDIS_HOST", "localhost")
    port = int(env.get(f"{prefix}REDIS_PORT", "6379"))
    password = env.get(f"{prefix}REDISCLI_AUTH") or None
    username = env.get(f"{prefix}REDIS_USER") or None
    db = int(env.get(f"{prefix}REDIS_DB", "0"))
    options: dict[str, Any] = {}
    decode_raw = env.get(f"{prefix}REDIS_DECODE_RESPONSES")
    if decode_raw is not None:
        options["decode_responses"] = _parse_bool(decode_raw, True)
    return RedisConnParams(
        host=host,
        port=port,
        password=password,
        db=db,
        username=username,
        options=options,
    )
