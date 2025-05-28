'''
Date: 2025-01-24 13:55:33
Author: mental1104 mental1104@gmail.com
LastEditors: mental1104 mental1104@gmail.com
LastEditTime: 2025-01-24 22:55:51
'''
import asyncio
from aiohttp import ClientSession
from mental1104.timed import async_timed


@async_timed
async def fetch_status(session: ClientSession, url: str, delay: int = 0) -> int:
    await asyncio.sleep(delay)
    async with session.get(url) as result:
        return result.status
