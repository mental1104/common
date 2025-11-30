import importlib
from types import ModuleType

from ._strategy import BackendUnavailable, Strategy

MODULE_NAME = "export_pybind"


class PyBindUnavailable(BackendUnavailable):
    def __init__(self, reason: str):
        super().__init__(backend="pybind11", reason=reason)


class PyBindStrategy(Strategy):
    name = "pybind11"

    def __init__(self):
        try:
            self._mod: ModuleType = importlib.import_module(MODULE_NAME)
        except ModuleNotFoundError as exc:
            raise PyBindUnavailable("module export_pybind not found; build it via CMake") from exc
        if not hasattr(self._mod, "parse_json"):
            raise PyBindUnavailable("export_pybind.parse_json not found; rebuild bindings")

    def parse_json(self, payload: str) -> tuple[bool, str, int]:
        ok, error, offset = self._mod.parse_json(payload)
        return bool(ok), str(error), int(offset)
