import httpx
import pytest
from fastapi import FastAPI

import mental1104.utils.context as ctx_mod
from mental1104.asgi.fastapi.middleware import (
    RequestCtxContextVarMiddlewareFactory,
    register_request_ctx_middleware,
)


@pytest.fixture(autouse=True)
def _ensure_ctx_is_clean():
    # Guard against cross-test leakage
    assert ctx_mod.ctx_diag()["is_set"] is False, (
        f"ctx leaked into test start: {ctx_mod.ctx_diag()}"
    )
    yield
    assert ctx_mod.ctx_diag()["is_set"] is False, f"ctx leaked after test end: {ctx_mod.ctx_diag()}"


@pytest.mark.asyncio
async def test_register_request_ctx_middleware_injects_and_resets():
    """Middleware should inject per-request ctx from headers and reset afterward."""

    app = FastAPI()
    register_request_ctx_middleware(app, RequestCtxContextVarMiddlewareFactory)

    @app.get("/whoami")
    async def whoami():
        # Handler reads current context set by middleware
        c = ctx_mod.ctx()
        return {"language": c.language, "time_zone": c.time_zone}

    transport = httpx.ASGITransport(app=app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        # Happy path: both lang and timezone provided
        r1 = await client.get("/whoami", headers={"lang": "en-US", "timezone": "Asia/Tokyo"})
        assert r1.json() == {"language": "en-US", "time_zone": "Asia/Tokyo"}
        assert ctx_mod.ctx_diag()["is_set"] is False  # Must reset after request

        # Missing timezone should fall back to RequestCtx defaults
        r2 = await client.get("/whoami", headers={"lang": "fr-FR"})
        assert r2.json() == {"language": "fr-FR", "time_zone": "Asia/Shanghai"}
        assert ctx_mod.ctx_diag()["is_set"] is False
