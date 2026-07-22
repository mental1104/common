from __future__ import annotations


class ExecutorError(RuntimeError):
    """Base error for reusable executor failures."""


class ExecutorTimeoutError(ExecutorError, TimeoutError):
    """The caller stopped waiting for a submitted executor task."""


class ExecutorTaskError(ExecutorError):
    """The underlying task failed while running in the executor."""


class ExecutorRejectedError(ExecutorError):
    """The task could not enter executor capacity within queue_timeout."""
