from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path

from ._strategy import BackendUnavailable, Strategy

LIB_ENV = "EXPORT_LAYER_CTYPE_LIB"
FUNC_NAME = "export_parse_json"


def _default_candidates() -> list[Path]:
    candidates: list[Path] = []
    here = Path(__file__).resolve()
    export_root = here.parents[3]  # .../common/export
    build_roots = [
        export_root / "cpp" / "build",
        export_root / "cpp" / "build" / "Debug",
        export_root / "cpp" / "build" / "Release",
    ]
    exts = [".so", ".dylib", ".dll"]
    for root in build_roots:
        for ext in exts:
            candidates.append(root / f"libexport_json{ext}")
            candidates.append(root / f"export_json{ext}")
    return candidates


class CTypesUnavailable(BackendUnavailable):
    def __init__(self, reason: str):
        super().__init__(backend="ctypes", reason=reason)


class CTypesStrategy(Strategy):
    name = "ctypes"

    def __init__(self, lib_path: str | None = None):
        target = self._resolve_path(lib_path)
        try:
            self._lib = ctypes.CDLL(str(target))
        except OSError as exc:
            raise CTypesUnavailable(f"failed to load shared library: {target}") from exc

        try:
            json_func = getattr(self._lib, FUNC_NAME)
        except AttributeError as exc:
            raise CTypesUnavailable(f"{FUNC_NAME} not exported by {target}") from exc

        class _JsonResult(ctypes.Structure):
            _fields_ = [("ok", ctypes.c_int), ("error", ctypes.c_char_p), ("offset", ctypes.c_size_t)]

        json_func.argtypes = [ctypes.c_char_p]
        json_func.restype = _JsonResult
        self._json_func = json_func

    def _resolve_path(self, lib_path: str | None) -> Path:
        if lib_path:
            return Path(lib_path).expanduser().resolve()
        env_val = os.getenv("MENTAL1104_EXPORT_LAYER_CTYPE_LIB") or os.getenv(LIB_ENV)
        if env_val:
            return Path(env_val).expanduser().resolve()
        for candidate in _default_candidates():
            if candidate.exists():
                return candidate
        raise CTypesUnavailable("no shared library found; set MENTAL1104_EXPORT_LAYER_CTYPE_LIB (or legacy EXPORT_LAYER_CTYPE_LIB) to point to libexport_json.so")

    def parse_json(self, payload: str) -> tuple[bool, object | None, str, int]:
        res = self._json_func(payload.encode("utf-8"))
        ok = bool(res.ok)
        error = res.error.decode(sys.getdefaultencoding()) if res.error else ""
        return ok, None, error, int(res.offset)
