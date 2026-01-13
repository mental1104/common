from __future__ import annotations

from functools import wraps
import inspect
from threading import Lock
from typing import Optional, Type, TypeVar

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import Session

from .session_context import get_current_async_session, get_current_session


class SessionAwareDAO:
    """
    Resolve optional session arguments for DAO methods.

    - If a session is provided, it is used.
    - Otherwise, the session is taken from the contextvar set by the UoW.
    - If no session is available, RuntimeError is raised.
    """

    def _session(self, session: Optional[Session] = None) -> Session:
        if session is not None:
            return session
        current = get_current_session()
        if current is None:
            raise RuntimeError(
                "No current Session. Use session_scope()/tx_scope() or pass a session explicitly."
            )
        return current

    async def _asession(self, session: Optional[AsyncSession] = None) -> AsyncSession:
        if session is not None:
            return session
        current = get_current_async_session()
        if current is None:
            raise RuntimeError(
                "No current AsyncSession. Use async_session_scope()/async_tx_scope() or pass a session explicitly."
            )
        return current


class AutoSessionDAO(SessionAwareDAO):
    """
    Auto-inject a session into public DAO methods.

    - Each public method should accept a `db` parameter (position doesn't matter).
      Recommended: make it keyword-only at the end, e.g. `def create(self, name: str, *, db)`.
    - Callers normally do not pass `db`; it is injected from ContextVar.
    - To override, pass `session=...` or `db=...` as a keyword.
    - Prefix non-DB helper methods with "_" to skip wrapping.
    - Set `__singleton__ = True` (or use @singleton_dao) to reuse a global instance.
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
        if getattr(init, "_auto_session_wrapped", False):
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

        wrapped._auto_session_wrapped = True
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
        def wrapper(self, *args, session=None, **kwargs):
            explicit_db = kwargs.pop("db", None)
            if explicit_db is not None and session is not None:
                raise ValueError("pass only one of db=... or session=...")
            db = explicit_db if explicit_db is not None else (session or self._session())
            return func(self, *args, **kwargs, db=db)

        return wrapper

    @staticmethod
    def _wrap_async(func):
        @wraps(func)
        async def wrapper(self, *args, session=None, **kwargs):
            explicit_db = kwargs.pop("db", None)
            if explicit_db is not None and session is not None:
                raise ValueError("pass only one of db=... or session=...")
            if explicit_db is not None:
                db = explicit_db
            elif session is not None:
                db = session
            else:
                db = await self._asession()
            return await func(self, *args, **kwargs, db=db)

        return wrapper


def singleton_dao(cls):
    """
    Class decorator to enable singleton construction for AutoSessionDAO subclasses.
    """
    cls.__singleton__ = True
    return cls


TSessionDAO = TypeVar("TSessionDAO", bound=SessionAwareDAO)


def make_async_dao(
    sync_cls: Type[TSessionDAO], *, name: Optional[str] = None
) -> Type[SessionAwareDAO]:
    """
    Build an async DAO that proxies sync DAO methods via AsyncSession.run_sync().

    - Sync DAO methods should accept `db` (recommended keyword-only).
    - The async DAO is backed by an instance of the sync DAO.
    - Override the session via `db=...` or `session=...` (AsyncSession), else ContextVar is used.
    """
    if not inspect.isclass(sync_cls):
        raise TypeError("sync_cls must be a class")
    if not issubclass(sync_cls, SessionAwareDAO):
        raise TypeError("sync_cls must inherit SessionAwareDAO")

    def __init__(self, *args, **kwargs):
        self._sync = sync_cls(*args, **kwargs)

    def _make_wrapper(func):
        @wraps(func)
        async def wrapper(self, *args, **kwargs):
            explicit_db = kwargs.pop("db", None)
            explicit_session = kwargs.pop("session", None)
            if explicit_db is not None and explicit_session is not None:
                raise ValueError("pass only one of db=... or session=...")
            if explicit_db is not None:
                async_session = explicit_db
            elif explicit_session is not None:
                async_session = explicit_session
            else:
                async_session = await self._asession()
            if not hasattr(async_session, "run_sync"):
                raise TypeError("async dao proxy requires AsyncSession.run_sync()")
            return await async_session.run_sync(
                lambda sync_session: func(self._sync, *args, **kwargs, db=sync_session)
            )

        wrapper.__signature__ = inspect.signature(func)
        return wrapper

    attrs = {
        "__init__": __init__,
        "__doc__": f"Async proxy for {sync_cls.__qualname__}.",
        "__module__": sync_cls.__module__,
    }
    for method_name, attr in list(sync_cls.__dict__.items()):
        if method_name.startswith("_"):
            continue
        if isinstance(attr, (staticmethod, classmethod, property)):
            continue
        if not callable(attr):
            continue
        if inspect.iscoroutinefunction(attr):
            continue
        attrs[method_name] = _make_wrapper(attr)

    cls_name = name or f"Async{sync_cls.__name__}"
    return type(cls_name, (SessionAwareDAO,), attrs)
