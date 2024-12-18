import functools
import time
from typing import Callable, Any, List
import asyncio
from aiohttp import ClientSession

def async_timed():
    def wrapper(func: Callable) -> Callable:
        @functools.wraps(func)
        async def wrapped(*args, **kwargs) -> Any:
            print(f'starting {func} with args {args} {kwargs}')
            start = time.time()
            try:
                return await func(*args, **kwargs)
            finally:
                end = time.time()
                total = end - start
                print(f'finished {func} in {total:.4f} second(s)')
        return wrapped
    return wrapper


async def delay(delay_seconds: int) -> int:
    print(f'sleeping for {delay_seconds} second(s)')
    await asyncio.sleep(delay_seconds)
    print(f'finished sleeping for {delay_seconds} second(s)')
    return delay_seconds


@async_timed()
async def fetch_status(session: ClientSession, url: str, delay: int = 0) -> int:
    await asyncio.sleep(delay)
    async with session.get(url) as result:
        return result.status
    


class CoroutinePool:
    def __init__(self, loop, max_concurrent_task=5):
        self.loop = loop
        self.semaphore = asyncio.Semaphore(max_concurrent_task)
    
    async def worker(self, coro):
        async with self.semaphore:
            result = await coro()
            return result
    
    @async_timed()
    async def run_task_batch(self, coros: List[functools.partial]):
        tasks = []
        for coro in coros:
            task = self.loop.create_task(self.worker(coro))
            tasks.append(task)
            
        result = await asyncio.gather(*task)
        return result
    
    def run(self, coros: List[functools.partial]):
        return self.loop.run_until_complete(self.run_task_batch(coros))
