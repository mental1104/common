from __future__ import annotations

from abc import ABC, abstractmethod
import inspect
from typing import Awaitable, Callable, cast

from fastapi import FastAPI, Request
from starlette.responses import Response
from mental1104.asgi.fastapi.request import request_ctx_from_headers
from mental1104.utils.context import reset_ctx, set_ctx

MiddlewareCallable = Callable[[Request, Callable[[Request], Awaitable[Response]]], Awaitable[Response]]


class RequestCtxMiddlewareFactory(ABC):
    """抽象工厂：生成具体的 middleware；逻辑由子类实现。"""

    @abstractmethod
    def create(self) -> MiddlewareCallable:
        raise NotImplementedError


def _collect_factory_types() -> list[type[RequestCtxMiddlewareFactory]]:
    """收集所有非抽象的工厂子类，供批量注册使用。"""

    seen: set[type[RequestCtxMiddlewareFactory]] = set()
    concrete: list[type[RequestCtxMiddlewareFactory]] = []

    def visit(cls: type[RequestCtxMiddlewareFactory]) -> None:
        for sub in cls.__subclasses__():
            if sub in seen:
                continue
            seen.add(sub)
            visit(cast(type[RequestCtxMiddlewareFactory], sub))
            if not inspect.isabstract(sub):
                concrete.append(cast(type[RequestCtxMiddlewareFactory], sub))

    visit(RequestCtxMiddlewareFactory)
    concrete.sort(key=lambda c: f"{c.__module__}.{c.__qualname__}")
    return concrete


def _instantiate_factory(factory: RequestCtxMiddlewareFactory | type[RequestCtxMiddlewareFactory]) -> RequestCtxMiddlewareFactory:
    if isinstance(factory, RequestCtxMiddlewareFactory):
        return factory
    if inspect.isclass(factory) and issubclass(factory, RequestCtxMiddlewareFactory):
        if inspect.isabstract(factory):
            raise TypeError(f"Factory class {factory.__name__} is abstract and cannot be instantiated")
        return factory()
    raise TypeError(f"Unsupported factory: {factory!r}")


def register_request_ctx_middleware(
    app: FastAPI,
    factory: RequestCtxMiddlewareFactory | type[RequestCtxMiddlewareFactory],
) -> MiddlewareCallable:
    """将指定工厂生成的 middleware 注册到 app。"""

    fac = _instantiate_factory(factory)
    mw = fac.create()
    app.middleware("http")(mw)
    return mw


def register_all_request_ctx_middlewares(
    app: FastAPI,
) -> None:
    """发现并注册所有工厂子类生成的 middleware，无返回值。"""

    for factory_cls in _collect_factory_types():
        register_request_ctx_middleware(app, factory_cls)


class RequestCtxContextVarMiddlewareFactory(RequestCtxMiddlewareFactory):
    """将 RequestCtx 写入 contextvars 的默认实现。"""

    def create(self) -> MiddlewareCallable:
        async def middleware(request: Request, call_next: Callable[[Request], Awaitable[Response]]) -> Response:
            ctx = request_ctx_from_headers(request)
            token = set_ctx(ctx)
            try:
                return await call_next(request)
            finally:
                reset_ctx(token)

        return middleware
