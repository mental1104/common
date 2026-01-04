from __future__ import annotations

from concurrent.futures import Executor, ProcessPoolExecutor, ThreadPoolExecutor
from typing import TYPE_CHECKING, Optional

from mental1104.concurrency.sync_base import _BaseSyncWorkerPool

if TYPE_CHECKING:
    from mental1104.concurrency.types import MPStartMethod


class ThreadWorkerPool(_BaseSyncWorkerPool):
    """纯线程池封装：提交同步函数, 无事件循环语义。"""

    def __init__(self, max_workers: Optional[int] = None):
        super().__init__(max_workers=max_workers)

    def _create_executor(self, max_workers: int) -> Executor:
        return ThreadPoolExecutor(max_workers=max_workers)


class ProcessWorkerPool(_BaseSyncWorkerPool):
    """纯进程池封装：提交同步函数, 适合完全同步/CPU 密集任务。"""

    def __init__(
        self,
        max_workers: Optional[int] = None,
        mp_start_method: Optional[MPStartMethod] = None,
    ):
        self._mp_start_method = mp_start_method
        super().__init__(max_workers=max_workers)

    def _create_executor(self, max_workers: int) -> Executor:
        if self._mp_start_method:
            import multiprocessing as mp

            ctx = mp.get_context(self._mp_start_method.value)
            return ProcessPoolExecutor(max_workers=max_workers, mp_context=ctx)
        return ProcessPoolExecutor(max_workers=max_workers)
