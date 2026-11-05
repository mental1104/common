"""延迟选择并缓存 C++ JSON 的 Python 调用策略。

默认按 pybind11、ctypes 顺序尝试；环境变量可指定策略并控制失败后的回退。
choose_strategy 只创建指定实例，不替换 parse_json 使用的默认缓存。
"""

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
    """选择可用策略并更新默认缓存，在候选失败时记录诊断。

    Returns:
        首个成功创建的策略实例，由后续默认调用复用。
    Raises:
        StrategyError: 所有允许尝试的策略均不可用或名称无效。
    """
    global _default_strategy
    # 带 MENTAL1104_ 前缀的设置优先，旧 EXPORT_LAYER_ 名称继续兼容。
    env_choice = os.getenv("MENTAL1104_EXPORT_LAYER_STRATEGY") or os.getenv("EXPORT_LAYER_STRATEGY", "")
    env_choice = env_choice.lower().strip()
    # 只有值为 "0" 才禁用显式策略失败后的回退；未指定策略时仍依序发现后端。
    allow_fallback_val = os.getenv("MENTAL1104_EXPORT_LAYER_ALLOW_FALLBACK")
    if allow_fallback_val is None:
        allow_fallback_val = os.getenv("EXPORT_LAYER_ALLOW_FALLBACK", "1")
    allow_fallback = allow_fallback_val != "0"
    candidates = [env_choice] if env_choice else ["pybind11", "ctypes"]

    errors = []

    def _try_strategy(factory: type[Strategy], *, log_prefix: str) -> Strategy | None:
        """创建一个候选策略，将不可用原因加入本轮收集的错误列表。

        Args:
            factory: 无参创建候选后端的策略类。
            log_prefix: 成功时使用的日志格式，包含策略名称占位符。
        Returns:
            可用实例；后端不可用时记录警告并返回 None。
        """
        try:
            strategy = factory()
        except BackendUnavailable as exc:
            # 记录当前候选不可用，继续选择；全部失败后才统一抛出策略错误。
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

    # 显式选择失败且允许回退时，重新按默认顺序发现可用后端。
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
