from __future__ import annotations

import os
from typing import Any, Mapping, Optional

from pymongo import MongoClient

from .config import MongoConnParams, mongo_params_from_env
from .context import (
    AsyncMongoSession,
    MongoSession,
    reset_current_async_mongo_session,
    reset_current_mongo_session,
    set_current_async_mongo_session,
    set_current_mongo_session,
)
from .factory import create_async_mongo_client, create_mongo_client
from .registry import DEFAULT_MONGO_REGISTRY, MongoRegistry


class MongoConnection:
    """
    Mongo connection context manager with ContextVar injection.
    """

    def __init__(
        self,
        host: Optional[str] = None,
        port: Optional[int] = None,
        user: Optional[str] = None,
        password: Optional[str] = None,
        database: Optional[str] = None,
        *,
        name: str = "default",
        registry: MongoRegistry = DEFAULT_MONGO_REGISTRY,
        params: Optional[MongoConnParams] = None,
        url: Optional[str] = None,
        options: Optional[Mapping[str, Any]] = None,
    ) -> None:
        self._token = None
        self._owned = False
        self._client: Optional[MongoClient] = None
        self._db_name: Optional[str] = database

        if params is None and url is None and any(v is not None for v in (host, port, user, password, database)):
            params = MongoConnParams(
                host=host or os.environ.get("MONGO_HOST", "localhost"),
                port=int(port or os.environ.get("MONGO_PORT", 27017)),
                database=database or os.environ.get("MONGO_DATABASE", "test"),
                user=user or os.environ.get("MONGO_USER") or None,
                password=password or os.environ.get("MONGO_PASSWORD") or None,
                options={},
            )

        if params is not None or url is not None or options is not None:
            self._client, db_name = create_mongo_client(
                params=params or mongo_params_from_env(),
                url=url,
                options=options,
            )
            self._db_name = database or db_name
            self._owned = True
            return

        try:
            self._client, db_name = registry.get_client(name)
            self._db_name = database or db_name
        except KeyError:
            params = mongo_params_from_env()
            self._client, db_name = create_mongo_client(params=params)
            self._db_name = database or db_name
            self._owned = True

    def __enter__(self) -> MongoSession:
        if self._client is None:
            raise RuntimeError("Mongo client is not initialized")
        mongo_session = MongoSession(client=self._client, db=self._client[self._db_name or "test"])
        self._token = set_current_mongo_session(mongo_session)
        return mongo_session

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        if self._token is not None:
            reset_current_mongo_session(self._token)
        if self._owned and self._client is not None:
            try:
                self._client.close()
            except Exception:
                pass


class AsyncMongoConnection:
    """
    Async Mongo connection context manager with ContextVar injection.
    """

    def __init__(
        self,
        host: Optional[str] = None,
        port: Optional[int] = None,
        user: Optional[str] = None,
        password: Optional[str] = None,
        database: Optional[str] = None,
        *,
        name: str = "default",
        registry: MongoRegistry = DEFAULT_MONGO_REGISTRY,
        params: Optional[MongoConnParams] = None,
        url: Optional[str] = None,
        options: Optional[Mapping[str, Any]] = None,
    ) -> None:
        self._token = None
        self._owned = False
        self._client = None
        self._db_name: Optional[str] = database

        if params is None and url is None and any(v is not None for v in (host, port, user, password, database)):
            params = MongoConnParams(
                host=host or os.environ.get("MONGO_HOST", "localhost"),
                port=int(port or os.environ.get("MONGO_PORT", 27017)),
                database=database or os.environ.get("MONGO_DATABASE", "test"),
                user=user or os.environ.get("MONGO_USER") or None,
                password=password or os.environ.get("MONGO_PASSWORD") or None,
                options={},
            )

        if params is not None or url is not None or options is not None:
            self._client, db_name = create_async_mongo_client(
                params=params or mongo_params_from_env(),
                url=url,
                options=options,
            )
            self._db_name = database or db_name
            self._owned = True
            return

        try:
            self._client, db_name = registry.get_async_client(name)
            self._db_name = database or db_name
        except KeyError:
            params = mongo_params_from_env()
            self._client, db_name = create_async_mongo_client(params=params)
            self._db_name = database or db_name
            self._owned = True

    async def __aenter__(self) -> AsyncMongoSession:
        if self._client is None:
            raise RuntimeError("Async Mongo client is not initialized")
        mongo_session = AsyncMongoSession(client=self._client, db=self._client[self._db_name or "test"])
        self._token = set_current_async_mongo_session(mongo_session)
        return mongo_session

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        if self._token is not None:
            reset_current_async_mongo_session(self._token)
        if self._owned and self._client is not None:
            try:
                self._client.close()
            except Exception:
                pass
