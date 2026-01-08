from __future__ import annotations

from contextvars import ContextVar, Token
from typing import Any, Optional

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import Session

_current_session: ContextVar[Optional[Session]] = ContextVar(
    "current_sqlalchemy_session", default=None
)
_current_async_session: ContextVar[Optional[AsyncSession]] = ContextVar(
    "current_sqlalchemy_async_session", default=None
)
_current_clickhouse_session: ContextVar[Optional[Any]] = ContextVar(
    "current_clickhouse_session", default=None
)


def set_current_session(session: Session) -> Token[Optional[Session]]:
    return _current_session.set(session)


def reset_current_session(token: Token[Optional[Session]]) -> None:
    _current_session.reset(token)


def get_current_session() -> Optional[Session]:
    return _current_session.get()


def set_current_async_session(session: AsyncSession) -> Token[Optional[AsyncSession]]:
    return _current_async_session.set(session)


def reset_current_async_session(token: Token[Optional[AsyncSession]]) -> None:
    _current_async_session.reset(token)


def get_current_async_session() -> Optional[AsyncSession]:
    return _current_async_session.get()


def set_current_clickhouse_session(session: Any) -> Token[Optional[Any]]:
    return _current_clickhouse_session.set(session)


def reset_current_clickhouse_session(token: Token[Optional[Any]]) -> None:
    _current_clickhouse_session.reset(token)


def get_current_clickhouse_session() -> Optional[Any]:
    return _current_clickhouse_session.get()


def ctx_clickhouse_session() -> Optional[Any]:
    return get_current_clickhouse_session()


def ctx_session() -> Optional[Session]:
    return get_current_session()


def ctx_async_session() -> Optional[AsyncSession]:
    return get_current_async_session()


def require_ctx_session() -> Session:
    session = get_current_session()
    if session is None:
        raise RuntimeError("No current Session in ContextVar; use session_scope()/tx_scope().")
    return session


def require_ctx_async_session() -> AsyncSession:
    session = get_current_async_session()
    if session is None:
        raise RuntimeError(
            "No current AsyncSession in ContextVar; use async_session_scope()/async_tx_scope()."
        )
    return session
