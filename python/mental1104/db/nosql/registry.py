from __future__ import annotations

from dataclasses import dataclass, field
from threading import Lock
from typing import Any, Dict, Mapping, Optional, Tuple

from pymongo import MongoClient
from pymongo.uri_parser import parse_uri

from .config import MongoConnParams, mongo_params_from_env
from .factory import create_async_mongo_client, create_mongo_client


@dataclass(frozen=True)
class MongoConfig:
    name: str
    url: Optional[str] = None
    params: Optional[MongoConnParams] = None
    options: Mapping[str, Any] = field(default_factory=dict)
    database: str = "test"


class MongoRegistry:
    def __init__(self) -> None:
        self._configs: Dict[str, MongoConfig] = {}
        self._clients: Dict[str, Tuple[MongoClient, str]] = {}
        self._async_clients: Dict[str, Tuple[object, str]] = {}
        self._lock = Lock()

    def register(
        self,
        *,
        name: str = "default",
        url: Optional[str] = None,
        params: Optional[MongoConnParams] = None,
        options: Optional[Mapping[str, Any]] = None,
        allow_overwrite: bool = False,
    ) -> MongoConfig:
        if not url and params is None:
            params = mongo_params_from_env()
        if not allow_overwrite and name in self._configs:
            raise ValueError(f"mongo '{name}' already registered")
        merged_options = dict(options or {})
        database = merged_options.get("database")
        if database is None and params is not None:
            database = params.database
        if database is None and url:
            parsed = parse_uri(url)
            database = parsed.get("database") or "test"
        if database is None:
            database = "test"
        config = MongoConfig(
            name=name,
            url=url,
            params=params,
            options=merged_options,
            database=database,
        )
        self._configs[name] = config
        return config

    def get_config(self, name: str = "default") -> MongoConfig:
        try:
            return self._configs[name]
        except KeyError as exc:
            raise KeyError(f"mongo '{name}' is not registered") from exc

    def get_client(self, name: str = "default") -> Tuple[MongoClient, str]:
        if name in self._clients:
            return self._clients[name]
        with self._lock:
            if name in self._clients:
                return self._clients[name]
            cfg = self.get_config(name)
            client, db_name = create_mongo_client(
                params=cfg.params,
                url=cfg.url,
                options=cfg.options,
            )
            db_name = cfg.database or db_name
            self._clients[name] = (client, db_name)
            return client, db_name

    def get_async_client(self, name: str = "default") -> Tuple[object, str]:
        if name in self._async_clients:
            return self._async_clients[name]
        with self._lock:
            if name in self._async_clients:
                return self._async_clients[name]
            cfg = self.get_config(name)
            client, db_name = create_async_mongo_client(
                params=cfg.params,
                url=cfg.url,
                options=cfg.options,
            )
            db_name = cfg.database or db_name
            self._async_clients[name] = (client, db_name)
            return client, db_name

    def close(self, name: str = "default") -> None:
        entry = self._clients.pop(name, None)
        if entry is None:
            return
        client, _ = entry
        try:
            client.close()
        except Exception:
            pass
        async_entry = self._async_clients.pop(name, None)
        if async_entry is None:
            return
        async_client, _ = async_entry
        try:
            async_client.close()
        except Exception:
            pass

    def close_all(self) -> None:
        names = list(self._clients.keys())
        for name in names:
            self.close(name)


DEFAULT_MONGO_REGISTRY = MongoRegistry()


def register_mongo(
    *,
    name: str = "default",
    url: Optional[str] = None,
    params: Optional[MongoConnParams] = None,
    options: Optional[Mapping[str, Any]] = None,
    allow_overwrite: bool = False,
) -> MongoConfig:
    return DEFAULT_MONGO_REGISTRY.register(
        name=name,
        url=url,
        params=params,
        options=options,
        allow_overwrite=allow_overwrite,
    )


def get_mongo_client(name: str = "default") -> Tuple[MongoClient, str]:
    return DEFAULT_MONGO_REGISTRY.get_client(name)


def get_async_mongo_client(name: str = "default") -> Tuple[object, str]:
    return DEFAULT_MONGO_REGISTRY.get_async_client(name)
