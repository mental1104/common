from __future__ import annotations

import os
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Callable, List, Optional, TypeVar

if TYPE_CHECKING:
    from concurrent.futures import Executor

_T = TypeVar("_T")


class _BaseSyncWorkerPool(ABC):
    """同步执行器封装：直接提交可调用对象到线程/进程池, 不涉 asyncio。"""

    def __init__(self, max_workers: Optional[int] = None):
        if max_workers is None:
            max_workers = self._default_max_workers()
        if max_workers < 1:
            raise ValueError("max_workers must be >= 1")
        self._executor: Executor | None = self._create_executor(max_workers)

    @staticmethod
    def _default_max_workers() -> int:
        cpu_count = os.cpu_count() or 4
        return max(1, min(cpu_count, 32))

    @abstractmethod
    def _create_executor(self, max_workers: int) -> Executor:
        raise NotImplementedError

    def run(self, callables: List[Callable[[], _T]]) -> List[_T]:
        if self._executor is None:
            raise RuntimeError("executor already shut down")
        futures = [self._executor.submit(fn) for fn in callables]
        return [future.result() for future in futures]

    def shutdown(self, wait: bool = True) -> None:
        if self._executor is not None:
            try:
                self._executor.shutdown(wait=wait, cancel_futures=False)
            except TypeError:
                self._executor.shutdown(wait=wait)
            self._executor = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.shutdown()
        return False
