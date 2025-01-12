import functools
import time
from typing import Callable, Any
import logging

def async_timed():
    def wrapper(func: Callable) -> Callable:
        @functools.wraps(func)
        async def wrapped(*args, **kwargs) -> Any:
            logging.debug(f'starting {func} with args {args} {kwargs}')
            start = time.time()
            try:
                return await func(*args, **kwargs)
            finally:
                end = time.time()
                total = end - start
                logging.debug(f'finished {func} in {total:.4f} second(s)')
        return wrapped
    return wrapper


def timed():
    def wrapper(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapped(*args, **kwargs) -> Any:
            logging.debug(f'starting {func} with args {args} {kwargs}')
            start = time.time()
            try:
                return func(*args, **kwargs)
            finally:
                end = time.time()
                total = end - start
                logging.debug(f'finished {func} in {total:.4f} second(s)')
        return wrapped
    return wrapper


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
    from datetime import datetime
    time_formats = [
        "%Y-%m-%dT%H:%M:%S",          # ISO 格式
        "%Y-%m-%d %H:%M:%S",          # 常见时间格式
        "%Y-%m-%d",                   # 仅日期格式
        "%Y-%m-%dT%H:%M:%S.%f",       # ISO 带微秒格式
        "%Y-%m-%d %H:%M:%S.%f",       # 常见时间格式带微秒
        "%d-%m-%Y",                   # 英式日期格式
        "%m/%d/%Y",                   # 美式日期格式
        "%m/%d/%Y %H:%M:%S",          # 美式时间格式
        "%m-%d-%Y %H:%M:%S",          # 美式时间格式带时间
        "%d-%b-%Y",                   # 简写月份格式
        "%d %B %Y",                   # 完整月份格式
        "%b %d, %Y",                  # 美式简写月份
        "%I:%M:%S %p, %d %B %Y",      # 12 小时制时间和完整日期
        "%Y.%m.%d",                   # 点号分隔的日期
        "%H:%M:%S",                   # 仅时间格式
        "%H:%M:%S.%f",                # 带微秒的时间格式
    ]
    for fmt in time_formats:
        try:
            return datetime.strptime(time_str, fmt)
        except ValueError:
            continue
    raise ValueError(f"无效的时间格式: {time_str}")