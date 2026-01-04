from __future__ import annotations

from enum import Enum


class MPStartMethod(Enum):
    """multiprocessing 支持的启动方式。"""

    SPAWN = "spawn"
    FORK = "fork"
    FORKSERVER = "forkserver"
