import pytest
from sqlalchemy import select

from app.models.user import User
from mental1104.db import require_ctx_session, session_scope


def test_sync_uow_injects_session(sync_client, uow, user_dao):
    with uow():
        user_dao.create("alice")

    with session_scope(client=sync_client):
        session = require_ctx_session()
        rows = session.execute(select(User)).scalars().all()
    assert len(rows) == 1


def test_sync_dao_without_uow_raises(user_dao):
    with pytest.raises(RuntimeError):
        user_dao.create("bob")


def test_sync_explicit_session_overrides_context(sync_client, uow, user_dao):
    with uow():
        explicit = sync_client.SessionMaker()
        try:
            user_dao.create("x", session=explicit)
            explicit.rollback()
        finally:
            explicit.close()

    with session_scope(client=sync_client):
        session = require_ctx_session()
        rows = session.execute(select(User).where(User.name == "x")).scalars().all()
    assert rows == []
