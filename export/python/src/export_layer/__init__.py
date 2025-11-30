"""Python entrypoint for the export layer.

Provides a simple API to call C++ backends via pybind11 (preferred) or ctypes
fallback. Currently exposes `parse_json`, backed by the canonical
`mental1104::parse_json` implementation.
"""

from .interface import parse_json, choose_strategy, StrategyError, BackendUnavailable, get_active_strategy_name

__all__ = [
    "parse_json",
    "choose_strategy",
    "StrategyError",
    "BackendUnavailable",
    "get_active_strategy_name",
]
