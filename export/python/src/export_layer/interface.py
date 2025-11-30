import logging
import os
from typing import Optional

from ._strategy import Strategy, StrategyError, BackendUnavailable
from ._pybind_strategy import PyBindStrategy, PyBindUnavailable
from ._ctypes_strategy import CTypesStrategy, CTypesUnavailable

logger = logging.getLogger(__name__)

_default_strategy: Optional[Strategy] = None


def _init_default() -> Strategy:
    global _default_strategy
    # Allow users to force a strategy via ENV; default preference is pybind11 > ctypes.
    env_choice = os.getenv("EXPORT_LAYER_STRATEGY", "").lower().strip()
    allow_fallback = os.getenv("EXPORT_LAYER_ALLOW_FALLBACK", "1") != "0"
    if env_choice:
        candidates = [env_choice]
    else:
        candidates = ["pybind11", "ctypes"]

    errors = []
    for name in candidates:
        factory: type[Strategy]
        if name == "pybind11":
            factory = PyBindStrategy
        elif name == "ctypes":
            factory = CTypesStrategy
        else:
            errors.append(StrategyError(f"Unknown strategy name {name!r}"))
            continue

        try:
            _default_strategy = factory()
            logger.info("Using export_layer backend: %s", _default_strategy.name)
            return _default_strategy
        except BackendUnavailable as exc:
            errors.append(exc)
            logger.warning("export_layer backend unavailable: %s", exc)
            if env_choice and not allow_fallback:
                break

    # Re-try with fallback order if explicit env choice failed and fallback is allowed.
    if env_choice and allow_fallback and not _default_strategy:
        for factory in (PyBindStrategy, CTypesStrategy):
            try:
                _default_strategy = factory()
                logger.info("Using export_layer backend (fallback): %s", _default_strategy.name)
                return _default_strategy
            except BackendUnavailable as exc:
                errors.append(exc)
                logger.warning("export_layer backend unavailable: %s", exc)

    raise StrategyError("No usable export_layer backend", errors)


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
