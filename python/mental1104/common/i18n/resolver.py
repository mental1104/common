"""
Locale resolution strategies for web frameworks.

ChainResolver enforces ordering and validates resolvers up front to fail fast
instead of raising AttributeError during requests.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Iterable, Optional, Protocol

if TYPE_CHECKING:
    from starlette.requests import Request

from .runtime import normalize_locale


class LocaleResolver(Protocol):
    async def resolve(self, request: Request) -> Optional[str]:  # pragma: no cover - Protocol
        ...


class ChainResolver:
    """
    Compose resolvers by priority: the first non-empty value wins.
    """

    def __init__(self, resolvers: Iterable[LocaleResolver], default_locale: str = "zh"):
        self.resolvers = list(resolvers)
        for resolver in self.resolvers:
            if not hasattr(resolver, "resolve") or not callable(resolver.resolve):
                raise TypeError(f"Resolver {resolver!r} must expose an async `resolve` method")
        self.default_locale = normalize_locale(default_locale, default_locale)

    async def resolve(self, request: Request) -> str:
        for resolver in self.resolvers:
            candidate = await resolver.resolve(request)
            if candidate:
                return normalize_locale(candidate, self.default_locale)
        return self.default_locale


class QueryResolver:
    def __init__(self, key: str = "lang"):
        self.key = key

    async def resolve(self, request: Request) -> Optional[str]:
        return request.query_params.get(self.key) or None


class HeaderResolver:
    def __init__(self, key: str = "X-Locale"):
        self.key = key

    async def resolve(self, request: Request) -> Optional[str]:
        return request.headers.get(self.key) or None


class CookieResolver:
    def __init__(self, key: str = "locale"):
        self.key = key

    async def resolve(self, request: Request) -> Optional[str]:
        return request.cookies.get(self.key) or None
