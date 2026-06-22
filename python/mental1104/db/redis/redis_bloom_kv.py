from __future__ import annotations

import contextlib
from typing import Any, Mapping, Optional

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

    @staticmethod
    def _to_text(value: Any) -> str:
        if isinstance(value, bytes):
            return value.decode(errors="ignore")
        return str(value)

    @classmethod
    def _module_name(cls, module: Any) -> str:
        if isinstance(module, Mapping):
            for key in ("name", b"name"):
                if key in module:
                    return cls._to_text(module[key]).lower()
            return ""

        if not isinstance(module, (list, tuple)):
            return ""

        values = list(module)
        for item in values:
            if isinstance(item, Mapping):
                name = cls._module_name(item)
                if name:
                    return name
            if isinstance(item, (list, tuple)) and len(item) >= 2:
                if cls._to_text(item[0]).lower() == "name":
                    return cls._to_text(item[1]).lower()

        for idx in range(0, len(values) - 1, 2):
            if cls._to_text(values[idx]).lower() == "name":
                return cls._to_text(values[idx + 1]).lower()

        return ""

    def _check_and_init_bloom(self) -> bool:
        try:
            modules = self.client.execute_command("MODULE", "LIST")
        except redis.RedisError:
            return False

        has_bf = False
        for module in modules:
            if self._module_name(module) in {"bf", "redisbloom"}:
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
