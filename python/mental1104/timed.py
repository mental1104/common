"""Compatibility shim exposing timed helpers at legacy path."""
from __future__ import annotations

from .utils import timed as _timed_module

async_timed = _timed_module.async_timed
timed = _timed_module.timed
get_current_time = _timed_module.get_current_time
parse_time = _timed_module.parse_time

__all__ = getattr(
    _timed_module,
    "__all__",
    ["async_timed", "timed", "get_current_time", "parse_time"],
)
