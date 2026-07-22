"""提供与 HTTP 客户端实现无关的调用结果分类和观察装饰器。"""

from __future__ import annotations

import functools
import inspect
from dataclasses import dataclass
from enum import Enum
from typing import Any, Callable, Optional, TypeVar, cast


class HTTPOutcomeKind(str, Enum):
    """表示一次 HTTP 调用最终落在哪一层。"""

    SUCCESS = "success"
    HTTP_STATUS_FAILURE = "http_status_failure"
    NETWORK_FAILURE = "network_failure"


@dataclass(frozen=True)
class HTTPOutcome:
    """描述一次 HTTP 调用的可观察结果。

    Attributes:
        kind: 成功、HTTP 状态失败或网络失败。
        status_code: 已经获得 HTTP 响应时的状态码；网络失败时为 None。
        error: 网络失败时捕获的原始异常；已获得 HTTP 响应时为 None。
    """

    kind: HTTPOutcomeKind
    status_code: Optional[int] = None
    error: Optional[BaseException] = None


CallableT = TypeVar("CallableT", bound=Callable[..., Any])
HTTPOutcomeObserver = Callable[[HTTPOutcome], None]


def classify_http_outcome(
    response: Optional[Any] = None,
    error: Optional[BaseException] = None,
) -> HTTPOutcome:
    """区分已收到 HTTP 响应的状态失败和未收到有效响应的网络失败。

    Args:
        response: HTTP 客户端返回的响应对象。对象必须提供整数类型的
            ``status_code``（requests/httpx）或 ``status``（aiohttp/http.client）属性。
        error: HTTP 客户端在获得有效响应前抛出的异常。非 None 时网络失败优先。

    Returns:
        error 非 None 时返回网络失败；否则状态码大于等于 400 时返回 HTTP 状态失败，
        其余状态返回成功。

    Raises:
        ValueError: response 和 error 同时为 None。
        TypeError: response 没有可识别的整数状态码属性。
    """

    if error is not None:
        return HTTPOutcome(kind=HTTPOutcomeKind.NETWORK_FAILURE, error=error)

    if response is None:
        raise ValueError("http outcome: response and error must not both be None")

    status_code = _extract_status_code(response)
    if status_code >= 400:
        return HTTPOutcome(
            kind=HTTPOutcomeKind.HTTP_STATUS_FAILURE,
            status_code=status_code,
        )

    return HTTPOutcome(kind=HTTPOutcomeKind.SUCCESS, status_code=status_code)


def observe_http_outcomes(observer: HTTPOutcomeObserver) -> Callable[[CallableT], CallableT]:
    """装饰同步或异步 HTTP 调用函数，并在不改写结果语义的前提下上报分类结果。

    Args:
        observer: 每次调用结束后接收 HTTPOutcome 的回调。回调异常会继续向外传播，
            因此用于网关观测时应保持轻量、线程安全且不可抛出异常。

    Returns:
        装饰器。被装饰函数返回响应时原样返回；抛出异常时先上报网络失败再原样抛出。
    """

    def decorator(func: CallableT) -> CallableT:
        """根据目标函数是否为协程函数选择同步或异步包装器。"""

        if inspect.iscoroutinefunction(func):

            @functools.wraps(func)
            async def async_wrapper(*args: Any, **kwargs: Any) -> Any:
                """执行异步调用并上报成功、HTTP 状态失败或网络失败。"""

                try:
                    response = await func(*args, **kwargs)
                except Exception as exc:
                    # 客户端在获得有效响应前抛出的异常属于网络层失败，并保持原异常语义。
                    observer(classify_http_outcome(error=exc))
                    raise

                observer(classify_http_outcome(response=response))
                return response

            return cast(CallableT, async_wrapper)

        @functools.wraps(func)
        def sync_wrapper(*args: Any, **kwargs: Any) -> Any:
            """执行同步调用并上报成功、HTTP 状态失败或网络失败。"""

            try:
                response = func(*args, **kwargs)
            except Exception as exc:
                # 客户端在获得有效响应前抛出的异常属于网络层失败，并保持原异常语义。
                observer(classify_http_outcome(error=exc))
                raise

            observer(classify_http_outcome(response=response))
            return response

        return cast(CallableT, sync_wrapper)

    return decorator


def _extract_status_code(response: Any) -> int:
    """从常见 Python HTTP 响应对象中读取整数状态码。

    Args:
        response: 提供 ``status_code`` 或 ``status`` 属性的响应对象。

    Returns:
        响应状态码。

    Raises:
        TypeError: 两个属性都不存在，或属性值不是整数。
    """

    for attribute_name in ("status_code", "status"):
        status_code = getattr(response, attribute_name, None)
        # bool 是 int 的子类，但不能作为合法 HTTP 状态码使用。
        if isinstance(status_code, int) and not isinstance(status_code, bool):
            return status_code

    raise TypeError(
        "http outcome: response must expose an integer status_code or status attribute"
    )
