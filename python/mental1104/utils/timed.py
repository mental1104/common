from __future__ import annotations

import functools
import logging
import time
from datetime import datetime
from typing import Any, Callable, Optional

from mental1104.compat import ZoneInfo


# 无参的 async_timed 装饰器
def async_timed(func: Callable) -> Callable:
    """
    装饰器, 用于记录异步函数执行时间。
    该装饰器会在函数执行前记录开始时间, 在函数执行后记录结束时间, 并计算总耗时。
    使用示例：
        @async_timed
        async def my_async_function():
            # 异步函数逻辑
            pass
    该装饰器会在日志中输出函数名、参数、开始时间和总耗时。
    """

    @functools.wraps(func)
    async def wrapped(*args, **kwargs) -> Any:
        logging.debug(f"starting {func} with args {args} {kwargs}")
        start = time.time()
        try:
            return await func(*args, **kwargs)
        finally:
            end = time.time()
            total = end - start
            logging.debug(f"finished {func} in {total:.4f} second(s)")

    return wrapped


# 无参的 timed 装饰器
def timed(func: Callable) -> Callable:
    """
    装饰器, 用于记录函数执行时间。
    该装饰器会在函数执行前记录开始时间, 在函数执行后记录结束时间, 并计算总耗时。
    使用示例：
        @timed
        def my_function():
            # 函数逻辑
            pass
    该装饰器会在日志中输出函数名、参数、开始时间和总耗时。
    """

    @functools.wraps(func)
    def wrapped(*args, **kwargs) -> Any:
        logging.debug(f"starting {func} with args {args} {kwargs}")
        start = time.time()
        try:
            return func(*args, **kwargs)
        finally:
            end = time.time()
            total = end - start
            logging.debug(f"finished {func} in {total:.4f} second(s)")

    return wrapped


_DEFAULT_TIME_FORMAT = "%Y-%m-%d %H:%M:%S"


def get_current_time(fmt=_DEFAULT_TIME_FORMAT, zone="Asia/Shanghai", **kwargs):
    """获取当前时间, 以特定格式来进行格式化。
    该方法使用了 zoneinfo 模块来处理时区, 确保在不同的时区下获取正确的时间。
    该方法默认使用 "%Y-%m-%d %H:%M:%S" 格式和 "Asia/Shanghai" 时区。
    如果需要其他格式或时区, 可以通过参数来指定。
    例如, 调用 `TimeHelper.get_current_time(format="%Y-%m-%d", zone="UTC")` 将返回当前 UTC 时间的日期部分。
    该方法返回一个字符串, 表示当前时间的格式化结果。

    Args:
        fmt (str, optional): 格式. Defaults to "%Y-%m-%d %H:%M:%S".
        zone (str, optional): 时区. Defaults to "Asia/Shanghai".

    Returns:
        str: 符合特定时区的时间字符串
    """
    if "format" in kwargs:
        if fmt != _DEFAULT_TIME_FORMAT:
            raise TypeError("get_current_time() got multiple values for format")
        fmt = kwargs.pop("format")
    if kwargs:
        extra = ", ".join(sorted(kwargs.keys()))
        raise TypeError(f"get_current_time() got unexpected keyword(s): {extra}")
    return datetime.now(ZoneInfo(zone)).strftime(fmt)


def parse_time(time_str):
    """
    将字符串解析为 datetime 对象。

    Args:
        time_str (str): 时间字符串。

    Returns:
        datetime: 解析成功的 datetime 对象。

    Raises:
        ValueError: 如果没有匹配的时间格式。
    """
    time_formats = [
        "%Y-%m-%dT%H:%M:%S",  # ISO 格式
        "%Y-%m-%d %H:%M:%S",  # 常见时间格式
        "%Y-%m-%d",  # 仅日期格式
        "%Y-%m-%dT%H:%M:%S.%f",  # ISO 带微秒格式
        "%Y-%m-%d %H:%M:%S.%f",  # 常见时间格式带微秒
        "%d-%m-%Y",  # 英式日期格式
        "%m/%d/%Y",  # 美式日期格式
        "%m/%d/%Y %H:%M:%S",  # 美式时间格式
        "%m-%d-%Y %H:%M:%S",  # 美式时间格式带时间
        "%d-%b-%Y",  # 简写月份格式
        "%d %B %Y",  # 完整月份格式
        "%b %d, %Y",  # 美式简写月份
        "%I:%M:%S %p, %d %B %Y",  # 12 小时制时间和完整日期
        "%Y.%m.%d",  # 点号分隔的日期
        "%H:%M:%S",  # 仅时间格式
        "%H:%M:%S.%f",  # 带微秒的时间格式
    ]

    def _try_parse(fmt: str) -> Optional[datetime]:
        try:
            return datetime.strptime(time_str, fmt).replace(
                tzinfo=datetime.now().astimezone().tzinfo
            )
        except ValueError:
            return None

    for fmt in time_formats:
        parsed = _try_parse(fmt)
        if parsed is not None:
            return parsed.replace(tzinfo=ZoneInfo("UTC"))
    raise ValueError(f"无效的时间格式: {time_str}")
