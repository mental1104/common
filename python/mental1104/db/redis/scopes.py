from __future__ import annotations

from contextlib import contextmanager
from typing import Iterator, Optional

import redis

from .context import reset_current_redis_client, set_current_redis_client
from .registry import DEFAULT_REDIS_REGISTRY, RedisRegistry


@contextmanager
def redis_session_scope(
    name: str = "default",
    *,
    registry: RedisRegistry = DEFAULT_REDIS_REGISTRY,
    client: Optional[redis.Redis] = None,
) -> Iterator[redis.Redis]:
    if client is None:
        client = registry.get_client(name)
    token = set_current_redis_client(client)
    try:
        yield client
    finally:
        reset_current_redis_client(token)


@contextmanager
def redis_tx_scope(
    name: str = "default",
    *,
    registry: RedisRegistry = DEFAULT_REDIS_REGISTRY,
    client: Optional[redis.Redis] = None,
) -> Iterator[redis.Redis]:
    with redis_session_scope(name=name, registry=registry, client=client) as redis_client:
        yield redis_client
