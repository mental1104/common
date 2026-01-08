from __future__ import annotations

import os
import time
import uuid
from typing import Any, Mapping, Optional

import redis

from .config import RedisConnParams, RedisMode, redis_params_from_env
from .context import reset_current_redis_client, set_current_redis_client
from .factory import create_redis_client
from .registry import DEFAULT_REDIS_REGISTRY, RedisRegistry


class RedisConnection:
    """
    Redis connection context manager.

    - Prefer using register_redis()/redis_session_scope() for shared clients.
    - Falls back to env-based standalone client when no registry config exists.
    """

    def __init__(
        self,
        host: Optional[str] = None,
        port: Optional[int] = None,
        password: Optional[str] = None,
        decode_responses: bool = True,
        *,
        db: Optional[int] = None,
        username: Optional[str] = None,
        mode: Optional[RedisMode | str] = None,
        options: Optional[Mapping[str, Any]] = None,
        name: str = "default",
        registry: RedisRegistry = DEFAULT_REDIS_REGISTRY,
        params: Optional[RedisConnParams] = None,
        url: Optional[str] = None,
    ) -> None:
        self._token = None
        self._owned = False
        self._client: Optional[redis.Redis] = None

        if params is None and url is None and any(v is not None for v in (host, port, password, db, username)):
            params = RedisConnParams(
                host=host or os.environ.get("REDIS_HOST", "localhost"),
                port=int(port or os.environ.get("REDIS_PORT", 6379)),
                password=password if password not in (None, "") else None,
                db=int(db if db is not None else os.environ.get("REDIS_DB", 0)),
                username=username or os.environ.get("REDIS_USER") or None,
                options={"decode_responses": decode_responses},
            )

        if params is not None or url is not None or mode is not None or options is not None:
            self._client = create_redis_client(
                params=params or redis_params_from_env(),
                url=url,
                mode=mode or os.environ.get("REDIS_MODE", RedisMode.STANDALONE.value),
                options=options,
            )
            self._owned = True
            return

        try:
            self._client = registry.get_client(name)
        except KeyError:
            fallback_params = redis_params_from_env()
            fallback_options = dict(fallback_params.options or {})
            fallback_options.setdefault("decode_responses", decode_responses)
            self._client = create_redis_client(
                params=fallback_params,
                mode=os.environ.get("REDIS_MODE", RedisMode.STANDALONE.value),
                options=fallback_options,
            )
            self._owned = True

    def __enter__(self) -> redis.Redis:
        if self._client is None:
            raise RuntimeError("Redis client is not initialized")
        self._token = set_current_redis_client(self._client)
        return self._client

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        if self._token is not None:
            reset_current_redis_client(self._token)
        if self._owned and self._client is not None:
            try:
                self._client.close()
            except Exception:
                pass


class RedisLock:
    def __init__(self, redis_client: redis.Redis, name: str, lock_expire: int = 10):
        """
        :param redis_client: redis.Redis instance
        :param name: lock key
        :param lock_expire: lock ttl (seconds)
        """
        self.redis_client = redis_client
        self.name = name
        self.lock_expire = lock_expire
        self.lock_value = str(uuid.uuid4())

    def try_lock(self, wait_timeout: float = 5, retry_delay: float = 0.01) -> bool:
        end_time = time.time() + wait_timeout
        while time.time() < end_time:
            if self.redis_client.set(self.name, self.lock_value, nx=True, ex=self.lock_expire):
                return True
            time.sleep(retry_delay)
        return False

    def unlock(self):
        lua_script = """
        if redis.call("get", KEYS[1]) == ARGV[1] then
            return redis.call("del", KEYS[1])
        else
            return 0
        end
        """
        script = self.redis_client.register_script(lua_script)
        return script(keys=[self.name], args=[self.lock_value])

    def __enter__(self):
        if not self.try_lock():
            raise RuntimeError("acquire redis lock timeout")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.unlock()
