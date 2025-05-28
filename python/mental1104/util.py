import time
import os
import re
import asyncio
import random
from functools import singledispatch
from datetime import datetime
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad


def delay(delay_seconds: int) -> int:
    """
    延迟指定的秒数。
    :param delay_seconds: 延迟的秒数，必须为非负整数。
    :return: 返回延迟的秒数。
    :raises ValueError: 如果 delay_seconds 小于 0，则抛出 ValueError。
    :example:
        >>> delay(5)
        sleeping for 5 second(s)
        finished sleeping for 5 second(s)
        5
    """
    if delay_seconds < 0:
        raise ValueError("Delay time cannot be negative")
    print(f'sleeping for {delay_seconds} second(s)')
    time.sleep(delay_seconds)
    print(f'finished sleeping for {delay_seconds} second(s)')
    return delay_seconds


async def async_delay(delay_seconds: int) -> int:
    """
    异步延迟指定的秒数。
    :param delay_seconds: 延迟的秒数，必须为非负整数。
    :return: 返回延迟的秒数。
    :raises ValueError: 如果 delay_seconds 小于 0，则抛出 ValueError。
    :example:
        >>> await async_delay(5)
        sleeping for 5 second(s)
        finished sleeping for 5 second(s)
        5
    """
    if delay_seconds < 0:
        raise ValueError("Delay time cannot be negative")
    print(f'sleeping for {delay_seconds} second(s)')
    await asyncio.sleep(delay_seconds)
    print(f'finished sleeping for {delay_seconds} second(s)')
    return delay_seconds


