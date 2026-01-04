from __future__ import annotations

import importlib
from typing import TYPE_CHECKING

from ._strategy import BackendUnavailable, Strategy

if TYPE_CHECKING:
    from types import ModuleType

MODULE_NAME = "mental1104_export_layer_pybind"


class PyBindUnavailable(BackendUnavailable):
    def __init__(self, reason: str):
        super().__init__(backend="pybind11", reason=reason)


class PyBindStrategy(Strategy):
    name = "pybind11"

    def __init__(self):
        try:
            self._mod: ModuleType = importlib.import_module(MODULE_NAME)
        except ModuleNotFoundError as exc:
            raise PyBindUnavailable("module mental1104_export_layer_pybind not found; build it via CMake") from exc
        if not hasattr(self._mod, "parse_json"):
            raise PyBindUnavailable("mental1104_export_layer_pybind.parse_json not found; rebuild bindings")
        self._parse_value = getattr(self._mod, "parse_json_value", None)

    def parse_json(self, payload: str):
        if callable(self._parse_value):
            ok, value, error, offset = self._parse_value(payload)
            return bool(ok), value, str(error), int(offset)
        ok, error, offset = self._mod.parse_json(payload)
        return bool(ok), None, str(error), int(offset)
