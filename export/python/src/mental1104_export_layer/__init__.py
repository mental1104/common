"""通过 pybind11 或 ctypes 调用 C++ JSON 实现。

Python 导出协议仍为 parse_json，核心实现入口为 mental1104::load_json。
"""

from __future__ import annotations

import contextlib
import logging
import os


def _setup_logging() -> None:
    level_name = os.getenv("MENTAL1104_EXPORT_LAYER_LOG_LEVEL") or os.getenv("EXPORT_LAYER_LOG_LEVEL", "INFO")
    level_name = level_name.upper()
    level = getattr(logging, level_name, logging.INFO)
    root = logging.getLogger()
    if not root.handlers:
        logging.basicConfig(level=level)
    else:
        with contextlib.suppress(Exception):
            root.setLevel(level)

def _load_interface():
    from .interface import (
        BackendUnavailable,
        StrategyError,
        choose_strategy,
        get_active_strategy_name,
        parse_json,
    )

    return BackendUnavailable, StrategyError, choose_strategy, get_active_strategy_name, parse_json


_setup_logging()
BackendUnavailable, StrategyError, choose_strategy, get_active_strategy_name, parse_json = _load_interface()

__all__ = [
    "BackendUnavailable",
    "StrategyError",
    "choose_strategy",
    "get_active_strategy_name",
    "parse_json",
]
