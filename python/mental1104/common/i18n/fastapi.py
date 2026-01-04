"""
FastAPI integration: middleware injects locale into contextvars so handlers and
background tasks can read it without explicit parameters.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from starlette.middleware.base import BaseHTTPMiddleware

from .context import activate, reset_locale

if TYPE_CHECKING:
    from starlette.requests import Request
    from starlette.types import ASGIApp

    from .resolver import LocaleResolver


class I18nMiddleware(BaseHTTPMiddleware):
    def __init__(self, app: ASGIApp, resolver: LocaleResolver):
        super().__init__(app)
        self.resolver = resolver

    async def dispatch(self, request: Request, call_next):
        locale = await self.resolver.resolve(request)
        token = activate(locale)
        request.state.locale = locale
        try:
            return await call_next(request)
        finally:
            reset_locale(token)
