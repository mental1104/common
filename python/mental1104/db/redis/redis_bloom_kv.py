from __future__ import annotations

import contextlib
from typing import Optional

import redis

from .context import require_ctx_redis_client


class RedisBloom:
    """
    Bloom filter helper on Redis.

    If the server has no Bloom module, enabled=False and exists() always returns True.
    """

    def __init__(
        self,
        client: Optional[redis.Redis] = None,
        filter_key: str = "bf:kv",
        error_rate: float = 0.01,
        capacity: int = 1_000_000,
    ) -> None:
        self.client = client or require_ctx_redis_client()
        self.filter_key = filter_key
        self.error_rate = error_rate
        self.capacity = capacity
        self.enabled = self._check_and_init_bloom()

    def _check_and_init_bloom(self) -> bool:
        try:
            modules = self.client.execute_command("MODULE", "LIST")
        except redis.RedisError:
            return False

        has_bf = False
        for module in modules:
            if len(module) >= 2 and str(module[1]).lower() == "bf":
                has_bf = True
                break

        if not has_bf:
            return False

        try:
            self.client.execute_command(
                "BF.RESERVE",
                self.filter_key,
                self.error_rate,
                self.capacity,
            )
        except redis.ResponseError as exc:
            msg = str(exc).lower()
            if "item exists" in msg or "exists" in msg:
                pass
            else:
                return False
        except redis.RedisError:
            return False

        return True

    def add(self, item: str) -> None:
        if not self.enabled:
            return
        with contextlib.suppress(redis.RedisError):
            self.client.execute_command("BF.ADD", self.filter_key, item)

    def exists(self, item: str) -> bool:
        if not self.enabled:
            return True
        try:
            res = self.client.execute_command("BF.EXISTS", self.filter_key, item)
            return bool(res)
        except redis.RedisError:
            return True
