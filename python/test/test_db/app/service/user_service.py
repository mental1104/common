from __future__ import annotations

from app.dao.async_user_dao import AsyncUserDAO
from app.dao.user_dao import UserDAO
from mental1104.db import AsyncUnitOfWork, UnitOfWork


class UserService:
    def __init__(self, uow: UnitOfWork):
        self._uow = uow
        self._dao = UserDAO()

    def create_user(self, name: str) -> int:
        with self._uow():
            user = self._dao.create(name=name)
            return user.id


class AsyncUserService:
    def __init__(self, uow: AsyncUnitOfWork):
        self._uow = uow
        self._dao = AsyncUserDAO()

    async def create_user(self, name: str) -> int:
        async with self._uow():
            user = await self._dao.create(name=name)
            return user.id
