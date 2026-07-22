"""HTTP 调用结果分类与装饰器测试。"""

import asyncio
from dataclasses import dataclass
from typing import List

import pytest

from mental1104.network.http_failure import (
    HTTPOutcome,
    HTTPOutcomeKind,
    classify_http_outcome,
    observe_http_outcomes,
)


@dataclass
class StatusCodeResponse:
    """模拟 requests/httpx 风格的响应对象。"""

    status_code: int


@dataclass
class StatusResponse:
    """模拟 aiohttp/http.client 风格的响应对象。"""

    status: int


def test_classify_http_outcome_distinguishes_success_status_and_network_failures() -> None:
    """成功响应应保留状态码，429/503 与连接异常应按所在层级分类。"""

    success = classify_http_outcome(StatusCodeResponse(204))
    too_many_requests = classify_http_outcome(StatusCodeResponse(429))
    unavailable = classify_http_outcome(StatusResponse(503))
    network_error = EOFError("connection closed before response")
    disconnected = classify_http_outcome(error=network_error)

    assert success == HTTPOutcome(
        kind=HTTPOutcomeKind.SUCCESS,
        status_code=204,
    )
    assert too_many_requests == HTTPOutcome(
        kind=HTTPOutcomeKind.HTTP_STATUS_FAILURE,
        status_code=429,
    )
    assert unavailable == HTTPOutcome(
        kind=HTTPOutcomeKind.HTTP_STATUS_FAILURE,
        status_code=503,
    )
    assert disconnected == HTTPOutcome(
        kind=HTTPOutcomeKind.NETWORK_FAILURE,
        error=network_error,
    )


def test_classify_http_outcome_rejects_missing_or_invalid_response() -> None:
    """缺失响应或非整数状态码应快速失败，避免产生含糊分类。"""

    with pytest.raises(ValueError):
        classify_http_outcome()

    with pytest.raises(TypeError):
        classify_http_outcome(object())

    with pytest.raises(TypeError):
        classify_http_outcome(StatusCodeResponse(True))


def test_observe_http_outcomes_preserves_sync_response_and_exception() -> None:
    """同步装饰器应原样返回响应，并在网络异常后重新抛出同一异常对象。"""

    observed: List[HTTPOutcome] = []
    response = StatusCodeResponse(503)

    @observe_http_outcomes(observed.append)
    def request_status() -> StatusCodeResponse:
        """返回用于测试的 503 响应。"""

        return response

    network_error = EOFError("EOF")

    @observe_http_outcomes(observed.append)
    def request_eof() -> StatusCodeResponse:
        """模拟在获得响应前直接断连。"""

        raise network_error

    assert request_status() is response
    with pytest.raises(EOFError) as exc_info:
        request_eof()

    assert exc_info.value is network_error
    assert observed[0].kind is HTTPOutcomeKind.HTTP_STATUS_FAILURE
    assert observed[0].status_code == 503
    assert observed[1].kind is HTTPOutcomeKind.NETWORK_FAILURE
    assert observed[1].error is network_error


def test_observe_http_outcomes_supports_async_callables() -> None:
    """异步装饰器应等待原协程，并按同一规则上报 HTTP 状态失败。"""

    observed: List[HTTPOutcome] = []

    @observe_http_outcomes(observed.append)
    async def request_status() -> StatusResponse:
        """返回用于测试的异步 429 响应。"""

        await asyncio.sleep(0)
        return StatusResponse(429)

    response = asyncio.run(request_status())

    assert response.status == 429
    assert observed == [
        HTTPOutcome(
            kind=HTTPOutcomeKind.HTTP_STATUS_FAILURE,
            status_code=429,
        )
    ]
