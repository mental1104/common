import pytest
from fastapi import FastAPI, Request
from fastapi.testclient import TestClient

from mental1104.common.i18n.context import get_locale
from mental1104.common.i18n.fastapi import I18nMiddleware
from mental1104.common.i18n.resolver import ChainResolver, CookieResolver, HeaderResolver, QueryResolver


def test_chain_resolver_priority_and_fastapi_integration():
    app = FastAPI()
    resolver = ChainResolver([QueryResolver(), HeaderResolver(), CookieResolver()], default_locale="zh")
    app.add_middleware(I18nMiddleware, resolver=resolver)

    @app.get("/locale")
    async def locale_endpoint(request: Request):
        return {"ctx": get_locale(), "state": request.state.locale}

    client = TestClient(app)

    resp = client.get("/locale?lang=en", headers={"X-Locale": "ja"}, cookies={"locale": "fr"})
    assert resp.status_code == 200
    assert resp.json() == {"ctx": "en", "state": "en"}

    resp = client.get("/locale", headers={"X-Locale": "ja"})
    assert resp.json()["ctx"] == "ja"

    resp = client.get("/locale")
    assert resp.json()["ctx"] == "zh"


def test_chain_resolver_early_failure():
    class BadResolver:
        pass

    with pytest.raises(TypeError):
        ChainResolver([BadResolver()], default_locale="zh")
