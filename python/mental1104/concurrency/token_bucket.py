"""Thread-safe in-process token bucket rate limiter."""

import math
import threading
import time
from typing import Optional


class AcquireCancelledError(RuntimeError):
    """Raised when a token acquisition is cancelled before it succeeds."""


class TokenBucket:
    """A blocking, lazily refilled token bucket for one Python process."""

    def __init__(self, rate: float, capacity: int) -> None:
        rate_value = float(rate)
        if not math.isfinite(rate_value) or rate_value <= 0:
            raise ValueError("rate must be a finite positive number")
        if isinstance(capacity, bool) or not isinstance(capacity, int) or capacity <= 0:
            raise ValueError("capacity must be a positive integer")

        self._lock = threading.Lock()
        self._rate = rate_value
        self._capacity = float(capacity)
        self._tokens = float(capacity)
        self._last = time.monotonic()

    def acquire(self, cancel_event: Optional[threading.Event] = None) -> None:
        """Block until one token is acquired or cancellation is requested."""
        while True:
            if cancel_event is not None and cancel_event.is_set():
                raise AcquireCancelledError("token acquisition cancelled")

            with self._lock:
                now = time.monotonic()
                elapsed = now - self._last
                self._tokens = min(
                    self._capacity,
                    self._tokens + elapsed * self._rate,
                )
                self._last = now

                if self._tokens >= 1.0:
                    self._tokens -= 1.0
                    return

                wait_seconds = (1.0 - self._tokens) / self._rate

            if cancel_event is None:
                time.sleep(wait_seconds)
            elif cancel_event.wait(wait_seconds):
                raise AcquireCancelledError("token acquisition cancelled")

    def release(self) -> None:
        """Do nothing because consumed rate-limit tokens are not returned."""
