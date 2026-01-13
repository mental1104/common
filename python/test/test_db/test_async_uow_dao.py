import pytest
from sqlalchemy import select

from app.models.user import User
from mental1104.db import async_session_scope, require_ctx_async_session


@pytest.mark.asyncio
async def test_async_uow_injects_session(async_client, async_uow, async_user_dao):
    async with async_uow():
        await async_user_dao.create("alice")

    async with async_session_scope(client=async_client):
        session = require_ctx_async_session()
        result = await session.execute(select(User))
        rows = result.scalars().all()
    assert len(rows) == 1


@pytest.mark.asyncio
async def test_async_dao_without_uow_raises(async_user_dao):
    with pytest.raises(RuntimeError):
        await async_user_dao.create("bob")


@pytest.mark.asyncio
async def test_async_explicit_session_overrides_context(async_client, async_uow, async_user_dao):
    async with async_uow():
        explicit = async_client.SessionMaker()
        try:
            await async_user_dao.create("x", session=explicit)
            await explicit.rollback()
        finally:
            await explicit.close()

    async with async_session_scope(client=async_client):
        session = require_ctx_async_session()
        result = await session.execute(select(User).where(User.name == "x"))
        rows = result.scalars().all()
    assert rows == []
