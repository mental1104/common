from __future__ import annotations

import logging
import os
from typing import Optional

from ._ctypes_strategy import CTypesStrategy
from ._pybind_strategy import PyBindStrategy
from ._strategy import BackendUnavailable, Strategy, StrategyError

logger = logging.getLogger(__name__)

_default_strategy: Optional[Strategy] = None


def _init_default() -> Strategy:
    global _default_strategy
    # Allow users to force a strategy via ENV; default preference is pybind11 > ctypes.
    env_choice = os.getenv("MENTAL1104_EXPORT_LAYER_STRATEGY") or os.getenv("EXPORT_LAYER_STRATEGY", "")
    env_choice = env_choice.lower().strip()
    allow_fallback_val = os.getenv("MENTAL1104_EXPORT_LAYER_ALLOW_FALLBACK")
    if allow_fallback_val is None:
        allow_fallback_val = os.getenv("EXPORT_LAYER_ALLOW_FALLBACK", "1")
    allow_fallback = allow_fallback_val != "0"
    candidates = [env_choice] if env_choice else ["pybind11", "ctypes"]

    errors = []

    def _try_strategy(factory: type[Strategy], *, log_prefix: str) -> Strategy | None:
        try:
            strategy = factory()
        except BackendUnavailable as exc:
            errors.append(exc)
            logger.warning("mental1104_export_layer backend unavailable: %s", exc)
            return None
        logger.info(log_prefix, strategy.name)
        return strategy

    for name in candidates:
        factory: type[Strategy]
        if name == "pybind11":
            factory = PyBindStrategy
        elif name == "ctypes":
            factory = CTypesStrategy
        else:
            errors.append(StrategyError(f"Unknown strategy name {name!r}"))
            continue

        strategy = _try_strategy(factory, log_prefix="Using mental1104_export_layer backend: %s")
        if strategy is not None:
            _default_strategy = strategy
            return _default_strategy
        if env_choice and not allow_fallback:
            break

    # Re-try with fallback order if explicit env choice failed and fallback is allowed.
    if env_choice and allow_fallback and not _default_strategy:
        for factory in (PyBindStrategy, CTypesStrategy):
            strategy = _try_strategy(factory, log_prefix="Using mental1104_export_layer backend (fallback): %s")
            if strategy is not None:
                _default_strategy = strategy
                return _default_strategy

    raise StrategyError("No usable mental1104_export_layer backend", errors)


def choose_strategy(name: str):
    name = name.lower()
    if name == "pybind11":
        return PyBindStrategy()
    if name == "ctypes":
        return CTypesStrategy()
    raise StrategyError(f"Unknown strategy: {name}")


def get_active_strategy_name() -> str:
    if _default_strategy is None:
        _init_default()
    assert _default_strategy is not None
    return _default_strategy.name


def parse_json(payload: str):
    if _default_strategy is None:
        _init_default()
    assert _default_strategy is not None
    return _default_strategy.parse_json(payload)
