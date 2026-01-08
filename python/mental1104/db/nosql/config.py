from __future__ import annotations

import os
from dataclasses import dataclass, field
from typing import Any, Mapping, Optional


@dataclass(frozen=True)
class MongoConnParams:
    host: str
    port: int = 27017
    database: str = "test"
    user: Optional[str] = None
    password: Optional[str] = None
    options: Mapping[str, Any] = field(default_factory=dict)


def mongo_params_from_env(prefix: str = "") -> MongoConnParams:
    env = os.environ
    host = env.get(f"{prefix}MONGO_HOST", "localhost")
    port = int(env.get(f"{prefix}MONGO_PORT", "27017"))
    database = env.get(f"{prefix}MONGO_DATABASE", "test")
    user = env.get(f"{prefix}MONGO_USER") or None
    password = env.get(f"{prefix}MONGO_PASSWORD") or None

    options: dict[str, Any] = {}
    auth_source = env.get(f"{prefix}MONGO_AUTH_SOURCE")
    if auth_source:
        options["authSource"] = auth_source
    replica_set = env.get(f"{prefix}MONGO_REPLICA_SET")
    if replica_set:
        options["replicaSet"] = replica_set
    direct_conn = env.get(f"{prefix}MONGO_DIRECT_CONNECTION")
    if direct_conn is not None:
        options["directConnection"] = str(direct_conn).strip().lower() in (
            "1",
            "true",
            "yes",
            "on",
        )

    return MongoConnParams(
        host=host,
        port=port,
        database=database,
        user=user,
        password=password,
        options=options,
    )
