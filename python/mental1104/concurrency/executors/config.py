from __future__ import annotations

import os
from dataclasses import dataclass, field


def _cpu_count() -> int:
    return os.cpu_count() or 1


@dataclass(frozen=True)
class ExecutorConfig:
    io_workers: int = 32
    cpu_workers: int = field(default_factory=lambda: max(1, _cpu_count() // 2))
    io_max_inflight: int = 128
    cpu_max_inflight: int = field(default_factory=lambda: max(1, _cpu_count()))
    default_io_timeout: float = 10.0
    default_cpu_timeout: float = 3.0
    default_queue_timeout: float = 1.0

    def __post_init__(self) -> None:
        if self.io_workers < 1:
            raise ValueError("io_workers must be >= 1")
        if self.cpu_workers < 1:
            raise ValueError("cpu_workers must be >= 1")
        if self.io_max_inflight < 1:
            raise ValueError("io_max_inflight must be >= 1")
        if self.cpu_max_inflight < 1:
            raise ValueError("cpu_max_inflight must be >= 1")
        if self.default_io_timeout <= 0:
            raise ValueError("default_io_timeout must be > 0")
        if self.default_cpu_timeout <= 0:
            raise ValueError("default_cpu_timeout must be > 0")
        if self.default_queue_timeout <= 0:
            raise ValueError("default_queue_timeout must be > 0")
