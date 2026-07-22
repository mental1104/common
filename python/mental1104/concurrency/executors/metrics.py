from __future__ import annotations

from dataclasses import dataclass


@dataclass
class ExecutorMetrics:
    submitted_total: int = 0
    success_total: int = 0
    timeout_total: int = 0
    error_total: int = 0
    rejected_total: int = 0
    active_waiters: int = 0

    def snapshot(self) -> dict[str, int]:
        return {
            "submitted_total": self.submitted_total,
            "success_total": self.success_total,
            "timeout_total": self.timeout_total,
            "error_total": self.error_total,
            "rejected_total": self.rejected_total,
            "active_waiters": self.active_waiters,
        }
