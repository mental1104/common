from __future__ import annotations

from dataclasses import dataclass
from typing import List


class StrategyError(RuntimeError):
    def __init__(self, message: str, details: List[Exception] | None = None):
        super().__init__(message)
        self.details = details or []

    def __str__(self) -> str:  # pragma: no cover - diagnostic
        if not self.details:
            return super().__str__()
        joined = "; ".join(str(d) for d in self.details)
        return f"{super().__str__()} (causes: {joined})"


@dataclass
class BackendUnavailable(StrategyError):
    backend: str
    reason: str

    def __init__(self, backend: str, reason: str):
        self.backend = backend
        self.reason = reason
        super().__init__(f"{backend} unavailable: {reason}")


class Strategy:
    name: str

    def parse_json(self, payload: str) -> tuple[bool, object | None, str, int]:  # (ok, value, error, offset)
        raise NotImplementedError
