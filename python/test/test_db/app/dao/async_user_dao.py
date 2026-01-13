from __future__ import annotations

from app.dao.user_dao import UserDAO
from mental1104.db import make_async_dao

AsyncUserDAO = make_async_dao(UserDAO, name="AsyncUserDAO")
