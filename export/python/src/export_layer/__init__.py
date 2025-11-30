"""Python entrypoint for the export layer.

Provides a simple API to call C++ backends via pybind11 (preferred) or ctypes
fallback. Currently exposes `parse_json`, backed by the canonical
`mental1104::parse_json` implementation.
"""

from __future__ import annotations

import logging
import os


def _setup_logging() -> None:
    level_name = os.getenv("EXPORT_LAYER_LOG_LEVEL", "INFO").upper()
    level = getattr(logging, level_name, logging.INFO)
    root = logging.getLogger()
    if not root.handlers:
        logging.basicConfig(level=level)
    else:
        try:
            root.setLevel(level)
        except Exception:
            pass


_setup_logging()

from .interface import parse_json, choose_strategy, StrategyError, BackendUnavailable, get_active_strategy_name

__all__ = [
    "parse_json",
    "choose_strategy",
    "StrategyError",
    "BackendUnavailable",
    "get_active_strategy_name",
]
