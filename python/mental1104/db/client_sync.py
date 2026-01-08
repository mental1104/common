from __future__ import annotations

from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any, Dict, Iterator, Optional

from sqlalchemy import create_engine, text
from sqlalchemy.engine import Engine
from sqlalchemy.orm import Session, sessionmaker


@dataclass(frozen=True)
class SQLAlchemyClient:
    engine: Engine
    SessionMaker: sessionmaker

    @contextmanager
    def session_scope(self) -> Iterator[Session]:
        session: Session = self.SessionMaker()
        try:
            yield session
            session.commit()
        except Exception:
            session.rollback()
            raise
        finally:
            session.close()

    def ping(self) -> None:
        with self.engine.connect() as conn:
            conn.execute(text("SELECT 1"))

    def close(self) -> None:
        self.engine.dispose()


def make_sqlalchemy_client(
    url: str,
    *,
    echo: bool,
    pool_size: int,
    max_overflow: int,
    pool_timeout: int,
    pool_recycle: int,
    pool_pre_ping: bool,
    connect_args: Optional[Dict[str, Any]] = None,
) -> SQLAlchemyClient:
    engine = create_engine(
        url,
        echo=echo,
        pool_size=pool_size,
        max_overflow=max_overflow,
        pool_timeout=pool_timeout,
        pool_recycle=pool_recycle,
        pool_pre_ping=pool_pre_ping,
        future=True,
        connect_args=connect_args or {},
    )
    SessionMaker = sessionmaker(bind=engine, autoflush=False, expire_on_commit=False, future=True)
    return SQLAlchemyClient(engine=engine, SessionMaker=SessionMaker)
