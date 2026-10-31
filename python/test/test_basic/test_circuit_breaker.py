import asyncio
import threading

import pytest

from mental1104.concurrency.circuit_breaker import (
    CircuitBreaker,
    CircuitBreakerConfig,
    CircuitOpenError,
    CircuitOutcome,
    CircuitState,
    StateChangeReason,
)


class ManualClock:
    def __init__(self) -> None:
        self.value = 0.0
        self.lock = threading.Lock()

    def __call__(self) -> float:
        with self.lock:
            return self.value

    def advance(self, seconds: float) -> None:
        with self.lock:
            self.value += seconds


def config(**overrides):
    values = dict(
        window_seconds=10.0,
        minimum_requests=3,
        failure_rate_threshold=0.5,
        slow_call_duration_seconds=1.0,
        slow_call_rate_threshold=0.5,
        open_duration_seconds=5.0,
        half_open_max_probes=3,
        half_open_successes_to_close=3,
    )
    values.update(overrides)
    return CircuitBreakerConfig(**values)


def open_breaker(breaker: CircuitBreaker) -> None:
    for _ in range(3):
        breaker.try_acquire().record_failure()
    assert breaker.state is CircuitState.OPEN


def test_config_rejects_invalid_values():
    invalid = [
        {"window_seconds": 0},
        {"minimum_requests": 0},
        {"failure_rate_threshold": -0.1},
        {"failure_rate_threshold": 1.1},
        {"slow_call_duration_seconds": 0},
        {"slow_call_rate_threshold": -0.1},
        {"slow_call_rate_threshold": 1.1},
        {"open_duration_seconds": 0},
        {"half_open_max_probes": 0},
        {"half_open_successes_to_close": 0},
        {"half_open_max_probes": 2, "half_open_successes_to_close": 3},
    ]
    for overrides in invalid:
        with pytest.raises(ValueError):
            config(**overrides)


def test_minimum_requests_and_failure_rate_open_the_circuit():
    clock = ManualClock()
    breaker = CircuitBreaker(config(), clock=clock)

    breaker.try_acquire().record_failure()
    breaker.try_acquire().record_success()
    assert breaker.state is CircuitState.CLOSED

    breaker.try_acquire().record_failure()
    assert breaker.state is CircuitState.OPEN


def test_slow_successes_can_open_the_circuit():
    clock = ManualClock()
    breaker = CircuitBreaker(
        config(
            minimum_requests=2,
            failure_rate_threshold=1.0,
            slow_call_rate_threshold=0.5,
        ),
        clock=clock,
    )

    breaker.try_acquire().record_success()
    permit = breaker.try_acquire()
    clock.advance(1.0)
    permit.record_success()

    snapshot = breaker.snapshot()
    assert snapshot.state is CircuitState.OPEN


def test_ignored_business_errors_do_not_enter_closed_window():
    clock = ManualClock()
    breaker = CircuitBreaker(config(minimum_requests=2), clock=clock)

    breaker.try_acquire().record_ignored()
    breaker.try_acquire().record_failure()

    snapshot = breaker.snapshot()
    assert snapshot.window_requests == 1
    assert snapshot.window_failures == 1
    assert snapshot.state is CircuitState.CLOSED


def test_sliding_window_prunes_expired_samples():
    clock = ManualClock()
    breaker = CircuitBreaker(
        config(window_seconds=2.0, minimum_requests=3),
        clock=clock,
    )

    breaker.try_acquire().record_failure()
    clock.advance(2.1)
    breaker.try_acquire().record_success()

    snapshot = breaker.snapshot()
    assert snapshot.window_requests == 1
    assert snapshot.window_failures == 0


def test_open_rejects_without_calling_operation_and_can_fallback():
    clock = ManualClock()
    breaker = CircuitBreaker(config(), clock=clock)
    open_breaker(breaker)
    called = []

    def operation():
        called.append(True)
        return "downstream"

    with pytest.raises(CircuitOpenError):
        breaker.call(operation)
    assert not called

    value = breaker.call(operation, fallback=lambda _: "cached")
    assert value == "cached"
    assert not called


def test_half_open_allows_only_one_fixed_probe_round_then_closes():
    clock = ManualClock()
    breaker = CircuitBreaker(config(), clock=clock)
    open_breaker(breaker)
    clock.advance(5.0)

    permits = [breaker.try_acquire() for _ in range(3)]
    assert breaker.state is CircuitState.HALF_OPEN
    with pytest.raises(CircuitOpenError):
        breaker.try_acquire()

    permits[0].record_success()
    permits[1].record_ignored()
    assert breaker.state is CircuitState.HALF_OPEN
    permits[2].record_success()

    assert breaker.state is CircuitState.CLOSED


def test_half_open_failure_or_slow_call_reopens():
    clock = ManualClock()
    breaker = CircuitBreaker(config(), clock=clock)
    open_breaker(breaker)
    clock.advance(5.0)

    breaker.try_acquire().record_failure()
    assert breaker.state is CircuitState.OPEN

    clock.advance(5.0)
    permit = breaker.try_acquire()
    clock.advance(1.0)
    permit.record_success()
    assert breaker.state is CircuitState.OPEN


def test_stale_completion_does_not_pollute_new_generation():
    clock = ManualClock()
    breaker = CircuitBreaker(config(), clock=clock)

    stale = breaker.try_acquire()
    for _ in range(3):
        breaker.try_acquire().record_failure()
    assert breaker.state is CircuitState.OPEN

    stale.record_success()
    assert breaker.snapshot().window_requests == 0


def test_permit_can_only_complete_once():
    breaker = CircuitBreaker(config())
    permit = breaker.try_acquire()

    assert permit.complete(CircuitOutcome.SUCCESS)
    assert not permit.complete(CircuitOutcome.FAILURE)
    snapshot = breaker.snapshot()
    assert snapshot.window_requests == 1
    assert snapshot.window_failures == 0


def test_concurrent_half_open_acquisition_respects_probe_limit():
    clock = ManualClock()
    breaker = CircuitBreaker(config(), clock=clock)
    open_breaker(breaker)
    clock.advance(5.0)

    barrier = threading.Barrier(10)
    permits = []
    rejected = []
    lock = threading.Lock()

    def worker():
        barrier.wait()
        try:
            permit = breaker.try_acquire()
        except CircuitOpenError:
            with lock:
                rejected.append(True)
        else:
            with lock:
                permits.append(permit)

    threads = [threading.Thread(target=worker) for _ in range(10)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    assert len(permits) == 3
    assert len(rejected) == 7
    for permit in permits:
        permit.record_success()
    assert breaker.state is CircuitState.CLOSED


def test_state_change_listener_receives_reasons_and_exceptions_are_ignored():
    clock = ManualClock()
    changes = []

    def listener(change):
        changes.append(change)
        if change.reason is StateChangeReason.COOLDOWN_ELAPSED:
            raise RuntimeError("observer failure")

    breaker = CircuitBreaker(config(), clock=clock, on_state_change=listener)
    open_breaker(breaker)
    clock.advance(5.0)
    permits = [breaker.try_acquire() for _ in range(3)]
    for permit in permits:
        permit.record_success()

    assert [change.reason for change in changes] == [
        StateChangeReason.FAILURE_RATE,
        StateChangeReason.COOLDOWN_ELAPSED,
        StateChangeReason.HALF_OPEN_SUCCEEDED,
    ]


def test_call_classifier_ignores_business_error():
    breaker = CircuitBreaker(config(minimum_requests=1))

    class BusinessError(RuntimeError):
        pass

    with pytest.raises(BusinessError):
        breaker.call(
            lambda: (_ for _ in ()).throw(BusinessError("out of stock")),
            is_failure=lambda error: not isinstance(error, BusinessError),
        )

    assert breaker.snapshot().window_requests == 0
    assert breaker.state is CircuitState.CLOSED


def test_async_call_records_results_and_uses_async_fallback():
    clock = ManualClock()
    breaker = CircuitBreaker(config(), clock=clock)

    async def scenario():
        assert await breaker.call_async(lambda: asyncio.sleep(0, result="ok")) == "ok"
        breaker.try_acquire().record_failure()
        breaker.try_acquire().record_failure()
        assert breaker.state is CircuitState.OPEN

        called = []

        async def operation():
            called.append(True)
            return "downstream"

        async def fallback(_):
            return "cached"

        assert await breaker.call_async(operation, fallback=fallback) == "cached"
        assert not called

    asyncio.run(scenario())
