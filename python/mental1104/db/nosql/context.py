from __future__ import annotations

from contextvars import ContextVar, Token
from dataclasses import dataclass
from functools import wraps
import inspect
from threading import Lock
from typing import Any, Optional

from pymongo import MongoClient
from pymongo.client_session import ClientSession
from pymongo.database import Database


@dataclass
class MongoSession:
    client: MongoClient
    db: Database
    session: Optional[ClientSession] = None


@dataclass
class AsyncMongoSession:
    client: Any
    db: Any
    session: Optional[Any] = None


_current_mongo_session: ContextVar[Optional[MongoSession]] = ContextVar(
    "current_mongo_session", default=None
)
_current_async_mongo_session: ContextVar[Optional[AsyncMongoSession]] = ContextVar(
    "current_async_mongo_session", default=None
)


def set_current_mongo_session(session: MongoSession) -> Token[Optional[MongoSession]]:
    return _current_mongo_session.set(session)


def reset_current_mongo_session(token: Token[Optional[MongoSession]]) -> None:
    _current_mongo_session.reset(token)


def get_current_mongo_session() -> Optional[MongoSession]:
    return _current_mongo_session.get()


def ctx_mongo_session() -> Optional[MongoSession]:
    return get_current_mongo_session()


def require_ctx_mongo_session() -> MongoSession:
    session = get_current_mongo_session()
    if session is None:
        raise RuntimeError("No current Mongo session. Use mongo_session_scope() or MongoConnection().")
    return session


class MongoSessionAware:
    def _mongo(self) -> MongoSession:
        return require_ctx_mongo_session()


def set_current_async_mongo_session(session: AsyncMongoSession) -> Token[Optional[AsyncMongoSession]]:
    return _current_async_mongo_session.set(session)


def reset_current_async_mongo_session(token: Token[Optional[AsyncMongoSession]]) -> None:
    _current_async_mongo_session.reset(token)


def get_current_async_mongo_session() -> Optional[AsyncMongoSession]:
    return _current_async_mongo_session.get()


def ctx_async_mongo_session() -> Optional[AsyncMongoSession]:
    return get_current_async_mongo_session()


def require_ctx_async_mongo_session() -> AsyncMongoSession:
    session = get_current_async_mongo_session()
    if session is None:
        raise RuntimeError(
            "No current async Mongo session. Use async_mongo_session_scope() or AsyncMongoConnection()."
        )
    return session


class AsyncMongoSessionAware:
    def _amongo(self) -> AsyncMongoSession:
        return require_ctx_async_mongo_session()


class AutoMongoSessionDAO(MongoSessionAware, AsyncMongoSessionAware):
    """
    Auto-inject a MongoSession/AsyncMongoSession into public DAO methods.

    - Each public method should accept a `mongo` parameter (recommended keyword-only).
    - Callers normally do not pass `mongo`; it is injected from ContextVar.
    - To override, pass `mongo=...` (or `db=...` alias) explicitly.
    - Prefix non-DB helper methods with "_" to skip wrapping.
    - Set `__singleton__ = True` to reuse a global instance.
    """

    __singleton__ = False
    _singleton_lock = Lock()

    def __init_subclass__(cls, **kwargs) -> None:
        super().__init_subclass__(**kwargs)
        for name, attr in list(cls.__dict__.items()):
            if name.startswith("_"):
                continue
            if isinstance(attr, (staticmethod, classmethod, property)):
                continue
            if not callable(attr):
                continue
            if inspect.iscoroutinefunction(attr):
                setattr(cls, name, cls._wrap_async(attr))
            else:
                setattr(cls, name, cls._wrap_sync(attr))
        cls._wrap_init()

    @classmethod
    def _wrap_init(cls) -> None:
        init = cls.__init__
        if getattr(init, "_auto_mongo_wrapped", False):
            return

        @wraps(init)
        def wrapped(self, *args, **kwargs):
            if getattr(cls, "__singleton__", False) and getattr(
                self, "_singleton_initialized", False
            ):
                return
            init(self, *args, **kwargs)
            if getattr(cls, "__singleton__", False):
                self._singleton_initialized = True

        wrapped._auto_mongo_wrapped = True
        cls.__init__ = wrapped

    def __new__(cls, *args, **kwargs):
        if not getattr(cls, "__singleton__", False):
            return super().__new__(cls)
        if args or kwargs:
            raise TypeError(f"{cls.__name__} is singleton; do not pass constructor args")
        instance = getattr(cls, "_singleton_instance", None)
        if instance is None:
            with cls._singleton_lock:
                instance = getattr(cls, "_singleton_instance", None)
                if instance is None:
                    instance = super().__new__(cls)
                    cls._singleton_instance = instance
        return instance

    @staticmethod
    def _wrap_sync(func):
        @wraps(func)
        def wrapper(self, *args, mongo=None, **kwargs):
            explicit_db = kwargs.pop("db", None)
            if explicit_db is not None and mongo is not None:
                raise ValueError("pass only one of mongo=... or db=...")
            if mongo is None:
                mongo = explicit_db if explicit_db is not None else self._mongo()
            return func(self, *args, **kwargs, mongo=mongo)

        return wrapper

    @staticmethod
    def _wrap_async(func):
        @wraps(func)
        async def wrapper(self, *args, mongo=None, **kwargs):
            explicit_db = kwargs.pop("db", None)
            if explicit_db is not None and mongo is not None:
                raise ValueError("pass only one of mongo=... or db=...")
            if mongo is None:
                mongo = explicit_db if explicit_db is not None else self._amongo()
            return await func(self, *args, **kwargs, mongo=mongo)

        return wrapper
