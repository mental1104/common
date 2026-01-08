from __future__ import annotations

from typing import List, Optional

from sqlalchemy import select
from sqlalchemy.orm import Session

from app.models.user import User
from mental1104.db import AutoSessionDAO


class UserDAO(AutoSessionDAO):
    def create(self, name: str, *, db: Session) -> User:
        user = User(name=name)
        db.add(user)
        db.flush()
        return user

    def get(self, user_id: int, *, db: Session) -> Optional[User]:
        result = db.execute(select(User).where(User.id == user_id))
        return result.scalar_one_or_none()

    def list(self, limit: int = 100, *, db: Session) -> List[User]:
        result = db.execute(select(User).limit(limit))
        return list(result.scalars().all())
