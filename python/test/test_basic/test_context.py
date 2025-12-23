import asyncio
import concurrent.futures

import pytest

import mental1104.utils.context as ctx_mod


@pytest.fixture(autouse=True)
def _ensure_ctx_is_clean():
    # 防止用例间串号：开始/结束都应为空
    assert ctx_mod.ctx_diag()["is_set"] is False, f"ctx leaked into test start: {ctx_mod.ctx_diag()}"
    yield
    assert ctx_mod.ctx_diag()["is_set"] is False, f"ctx leaked after test end: {ctx_mod.ctx_diag()}"


def test_ctx_default_value():
    v = ctx_mod.ctx()
    assert v.language == "zh-CN"
    assert v.time_zone == "Asia/Shanghai"


def test_set_and_reset_ctx_roundtrip():
    token = ctx_mod.set_ctx(ctx_mod.RequestCtx(language="en-US", time_zone="Asia/Tokyo"))
    try:
        v = ctx_mod.ctx()
        assert v.language == "en-US"
        assert v.time_zone == "Asia/Tokyo"
    finally:
        ctx_mod.reset_ctx(token)

    v2 = ctx_mod.ctx()
    assert v2.language == "zh-CN"
    assert v2.time_zone == "Asia/Shanghai"


@pytest.mark.asyncio
async def test_ctx_is_isolated_between_async_tasks():
    async def worker(lang: str):
        token = ctx_mod.set_ctx(ctx_mod.RequestCtx(language=lang, time_zone="UTC"))
        try:
            await asyncio.sleep(0.01)  # 让任务交错执行
            return ctx_mod.ctx().language
        finally:
            ctx_mod.reset_ctx(token)

    a, b = await asyncio.gather(worker("en-US"), worker("ja-JP"))
    assert {a, b} == {"en-US", "ja-JP"}


def test_ctx_is_isolated_between_threads():
    def worker(lang: str) -> str:
        token = ctx_mod.set_ctx(ctx_mod.RequestCtx(language=lang, time_zone="UTC"))
        try:
            return ctx_mod.ctx().language
        finally:
            ctx_mod.reset_ctx(token)

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(worker, ["en-US", "ja-JP"]))

    assert set(results) == {"en-US", "ja-JP"}
    # 主线程不应残留上下文
    assert ctx_mod.ctx_diag()["is_set"] is False
