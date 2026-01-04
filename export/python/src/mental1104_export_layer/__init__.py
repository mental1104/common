"""Python entrypoint for the mental1104 export layer.

Provides a simple API to call C++ backends via pybind11 (preferred) or ctypes
fallback. Currently exposes `parse_json`, backed by the canonical
`mental1104::parse_json` implementation.
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
