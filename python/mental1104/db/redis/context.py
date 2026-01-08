from __future__ import annotations

from contextvars import ContextVar, Token
from typing import Any, Optional

_current_redis_client: ContextVar[Optional[Any]] = ContextVar("current_redis_client", default=None)


def set_current_redis_client(client: Any) -> Token[Optional[Any]]:
    return _current_redis_client.set(client)


def reset_current_redis_client(token: Token[Optional[Any]]) -> None:
    _current_redis_client.reset(token)


def get_current_redis_client() -> Optional[Any]:
    return _current_redis_client.get()


def ctx_redis_client() -> Optional[Any]:
    return get_current_redis_client()


def require_ctx_redis_client() -> Any:
    client = get_current_redis_client()
    if client is None:
        raise RuntimeError("No current Redis client. Use redis_session_scope() or RedisConnection().")
    return client


class RedisSessionAware:
    def _redis(self) -> Any:
        return require_ctx_redis_client()
