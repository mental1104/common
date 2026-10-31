from __future__ import annotations

import inspect
import threading
import time
from collections import deque
from dataclasses import dataclass
from enum import Enum
from typing import Awaitable, Callable, Deque, Generic, Optional, Tuple, TypeVar

T = TypeVar("T")


class CircuitState(Enum):
    CLOSED = "closed"
    OPEN = "open"
    HALF_OPEN = "half_open"


class CircuitOutcome(Enum):
    SUCCESS = "success"
    FAILURE = "failure"
    IGNORED = "ignored"


class StateChangeReason(Enum):
    FAILURE_RATE = "failure_rate"
    SLOW_CALL_RATE = "slow_call_rate"
    COOLDOWN_ELAPSED = "cooldown_elapsed"
    HALF_OPEN_SUCCEEDED = "half_open_succeeded"
    HALF_OPEN_FAILED = "half_open_failed"


class CircuitOpenError(RuntimeError):
    """Raised when a circuit breaker rejects a call without invoking downstream."""

    def __init__(self, retry_after_seconds: float) -> None:
        self.retry_after_seconds = max(0.0, retry_after_seconds)
        super().__init__(
            "circuit breaker is open; retry after {:.6f}s".format(
                self.retry_after_seconds
            )
        )


@dataclass(frozen=True)
class CircuitBreakerConfig:
    window_seconds: float = 10.0
    minimum_requests: int = 20
    failure_rate_threshold: float = 0.5
    slow_call_duration_seconds: float = 0.8
    slow_call_rate_threshold: float = 0.6
    open_duration_seconds: float = 5.0
    half_open_max_probes: int = 3
    half_open_successes_to_close: int = 3

    def __post_init__(self) -> None:
        if self.window_seconds <= 0:
            raise ValueError("window_seconds must be positive")
        if self.minimum_requests <= 0:
            raise ValueError("minimum_requests must be positive")
        if not 0.0 <= self.failure_rate_threshold <= 1.0:
            raise ValueError("failure_rate_threshold must be in [0, 1]")
        if self.slow_call_duration_seconds <= 0:
            raise ValueError("slow_call_duration_seconds must be positive")
        if not 0.0 <= self.slow_call_rate_threshold <= 1.0:
            raise ValueError("slow_call_rate_threshold must be in [0, 1]")
        if self.open_duration_seconds <= 0:
            raise ValueError("open_duration_seconds must be positive")
        if self.half_open_max_probes <= 0:
            raise ValueError("half_open_max_probes must be positive")
        if self.half_open_successes_to_close <= 0:
            raise ValueError("half_open_successes_to_close must be positive")
        if self.half_open_successes_to_close > self.half_open_max_probes:
            raise ValueError(
                "half_open_successes_to_close must not exceed "
                "half_open_max_probes"
            )


@dataclass(frozen=True)
class StateChange:
    previous_state: CircuitState
    new_state: CircuitState
    reason: StateChangeReason
    monotonic_time: float
    generation: int


@dataclass(frozen=True)
class CircuitBreakerSnapshot:
    state: CircuitState
    generation: int
    window_requests: int
    window_failures: int
    window_slow_calls: int
    failure_rate: float
    slow_call_rate: float
    half_open_issued: int
    half_open_in_flight: int
    half_open_successes: int
    retry_after_seconds: float


class CircuitPermit:
    """One-shot permission returned by :meth:`CircuitBreaker.try_acquire`."""

    def __init__(
        self,
        breaker: "CircuitBreaker",
        generation: int,
        state: CircuitState,
        started_at: float,
    ) -> None:
        self._breaker = breaker
        self._generation = generation
        self._state = state
        self._started_at = started_at
        self._completion_lock = threading.Lock()
        self._completed = False

    @property
    def state(self) -> CircuitState:
        return self._state

    def complete(self, outcome: CircuitOutcome) -> bool:
        if not isinstance(outcome, CircuitOutcome):
            raise TypeError("outcome must be a CircuitOutcome")

        with self._completion_lock:
            if self._completed:
                return False
            self._completed = True

        self._breaker._complete(
            generation=self._generation,
            acquired_state=self._state,
            started_at=self._started_at,
            outcome=outcome,
        )
        return True

    def record_success(self) -> bool:
        return self.complete(CircuitOutcome.SUCCESS)

    def record_failure(self) -> bool:
        return self.complete(CircuitOutcome.FAILURE)

    def record_ignored(self) -> bool:
        return self.complete(CircuitOutcome.IGNORED)


class CircuitBreaker:
    """Thread-safe local circuit breaker with an exact time sliding window."""

    def __init__(
        self,
        config: Optional[CircuitBreakerConfig] = None,
        *,
        on_state_change: Optional[Callable[[StateChange], None]] = None,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self._config = config or CircuitBreakerConfig()
        self._on_state_change = on_state_change
        self._clock = clock
        self._lock = threading.RLock()

        self._state = CircuitState.CLOSED
        self._generation = 0
        self._opened_at = 0.0
        self._events: Deque[Tuple[float, bool, bool]] = deque()

        self._half_open_issued = 0
        self._half_open_in_flight = 0
        self._half_open_successes = 0

    @property
    def config(self) -> CircuitBreakerConfig:
        return self._config

    @property
    def state(self) -> CircuitState:
        with self._lock:
            return self._state

    def try_acquire(self) -> CircuitPermit:
        change: Optional[StateChange] = None
        with self._lock:
            now = self._clock()

            if self._state is CircuitState.OPEN:
                retry_after = self._retry_after_locked(now)
                if retry_after > 0:
                    raise CircuitOpenError(retry_after)
                change = self._transition_locked(
                    CircuitState.HALF_OPEN,
                    StateChangeReason.COOLDOWN_ELAPSED,
                    now,
                )

            if self._state is CircuitState.HALF_OPEN:
                if self._half_open_issued >= self._config.half_open_max_probes:
                    raise CircuitOpenError(0.0)
                self._half_open_issued += 1
                self._half_open_in_flight += 1

            permit = CircuitPermit(
                breaker=self,
                generation=self._generation,
                state=self._state,
                started_at=now,
            )

        self._notify(change)
        return permit

    def snapshot(self) -> CircuitBreakerSnapshot:
        with self._lock:
            now = self._clock()
            if self._state is CircuitState.CLOSED:
                self._prune_events_locked(now)

            requests = len(self._events)
            failures = sum(1 for _, failed, _ in self._events if failed)
            slow_calls = sum(1 for _, _, slow in self._events if slow)
            failure_rate = failures / requests if requests else 0.0
            slow_call_rate = slow_calls / requests if requests else 0.0

            return CircuitBreakerSnapshot(
                state=self._state,
                generation=self._generation,
                window_requests=requests,
                window_failures=failures,
                window_slow_calls=slow_calls,
                failure_rate=failure_rate,
                slow_call_rate=slow_call_rate,
                half_open_issued=self._half_open_issued,
                half_open_in_flight=self._half_open_in_flight,
                half_open_successes=self._half_open_successes,
                retry_after_seconds=self._retry_after_locked(now),
            )

    def call(
        self,
        operation: Callable[[], T],
        *,
        is_failure: Optional[Callable[[Exception], bool]] = None,
        fallback: Optional[Callable[[CircuitOpenError], T]] = None,
    ) -> T:
        try:
            permit = self.try_acquire()
        except CircuitOpenError as error:
            if fallback is None:
                raise
            return fallback(error)

        try:
            result = operation()
        except BaseException as error:
            if isinstance(error, Exception):
                classifier = is_failure or (lambda _: True)
                try:
                    outcome = (
                        CircuitOutcome.FAILURE
                        if classifier(error)
                        else CircuitOutcome.IGNORED
                    )
                except BaseException:
                    permit.record_failure()
                    raise
                permit.complete(outcome)
            else:
                permit.record_ignored()
            raise

        permit.record_success()
        return result

    async def call_async(
        self,
        operation: Callable[[], Awaitable[T]],
        *,
        is_failure: Optional[Callable[[Exception], bool]] = None,
        fallback: Optional[Callable[[CircuitOpenError], Awaitable[T]]] = None,
    ) -> T:
        try:
            permit = self.try_acquire()
        except CircuitOpenError as error:
            if fallback is None:
                raise
            fallback_result = fallback(error)
            if not inspect.isawaitable(fallback_result):
                raise TypeError("async fallback must return an awaitable")
            return await fallback_result

        try:
            result = await operation()
        except BaseException as error:
            if isinstance(error, Exception):
                classifier = is_failure or (lambda _: True)
                try:
                    outcome = (
                        CircuitOutcome.FAILURE
                        if classifier(error)
                        else CircuitOutcome.IGNORED
                    )
                except BaseException:
                    permit.record_failure()
                    raise
                permit.complete(outcome)
            else:
                permit.record_ignored()
            raise

        permit.record_success()
        return result

    def _complete(
        self,
        *,
        generation: int,
        acquired_state: CircuitState,
        started_at: float,
        outcome: CircuitOutcome,
    ) -> None:
        change: Optional[StateChange] = None
        with self._lock:
            now = self._clock()
            if generation != self._generation or acquired_state is not self._state:
                return

            duration = max(0.0, now - started_at)
            slow = duration >= self._config.slow_call_duration_seconds

            if self._state is CircuitState.CLOSED:
                if outcome is CircuitOutcome.IGNORED:
                    return

                self._events.append(
                    (now, outcome is CircuitOutcome.FAILURE, slow)
                )
                self._prune_events_locked(now)
                change = self._evaluate_closed_locked(now)

            elif self._state is CircuitState.HALF_OPEN:
                self._half_open_in_flight -= 1
                unhealthy = outcome is CircuitOutcome.FAILURE or slow
                if unhealthy:
                    change = self._transition_locked(
                        CircuitState.OPEN,
                        StateChangeReason.HALF_OPEN_FAILED,
                        now,
                    )
                else:
                    self._half_open_successes += 1
                    if (
                        self._half_open_successes
                        >= self._config.half_open_successes_to_close
                        and self._half_open_in_flight == 0
                    ):
                        change = self._transition_locked(
                            CircuitState.CLOSED,
                            StateChangeReason.HALF_OPEN_SUCCEEDED,
                            now,
                        )

        self._notify(change)

    def _evaluate_closed_locked(self, now: float) -> Optional[StateChange]:
        requests = len(self._events)
        if requests < self._config.minimum_requests:
            return None

        failures = sum(1 for _, failed, _ in self._events if failed)
        slow_calls = sum(1 for _, _, slow in self._events if slow)
        if failures / requests >= self._config.failure_rate_threshold:
            return self._transition_locked(
                CircuitState.OPEN,
                StateChangeReason.FAILURE_RATE,
                now,
            )
        if slow_calls / requests >= self._config.slow_call_rate_threshold:
            return self._transition_locked(
                CircuitState.OPEN,
                StateChangeReason.SLOW_CALL_RATE,
                now,
            )
        return None

    def _prune_events_locked(self, now: float) -> None:
        cutoff = now - self._config.window_seconds
        while self._events and self._events[0][0] < cutoff:
            self._events.popleft()

    def _retry_after_locked(self, now: float) -> float:
        if self._state is not CircuitState.OPEN:
            return 0.0
        return max(
            0.0,
            self._opened_at + self._config.open_duration_seconds - now,
        )

    def _transition_locked(
        self,
        new_state: CircuitState,
        reason: StateChangeReason,
        now: float,
    ) -> StateChange:
        previous_state = self._state
        self._state = new_state
        self._generation += 1
        self._events.clear()
        self._half_open_issued = 0
        self._half_open_in_flight = 0
        self._half_open_successes = 0
        self._opened_at = now if new_state is CircuitState.OPEN else 0.0
        return StateChange(
            previous_state=previous_state,
            new_state=new_state,
            reason=reason,
            monotonic_time=now,
            generation=self._generation,
        )

    def _notify(self, change: Optional[StateChange]) -> None:
        if change is None or self._on_state_change is None:
            return
        try:
            self._on_state_change(change)
        except Exception:
            # Observability hooks must not affect state-machine correctness.
            return
