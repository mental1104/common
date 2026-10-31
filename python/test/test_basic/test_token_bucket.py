import math
import threading
import time

import pytest

from mental1104.concurrency.token_bucket import (
    AcquireCancelledError,
    TokenBucket,
    rate_limited,
)


def test_rejects_invalid_configuration():
    for rate in (0, -1, True, math.nan, math.inf, -math.inf):
        with pytest.raises(ValueError):
            TokenBucket(rate, 1)

    for capacity in (0, -1, True, 1.5):
        with pytest.raises(ValueError):
            TokenBucket(1, capacity)


def test_starts_full_and_replenishes_lazily():
    bucket = TokenBucket(rate=20, capacity=2)
    bucket.acquire()
    bucket.acquire()

    started = time.monotonic()
    bucket.acquire()
    elapsed = time.monotonic() - started

    assert elapsed >= 0.03
    assert elapsed < 0.5


def test_release_does_not_return_a_token():
    bucket = TokenBucket(rate=0.1, capacity=1)
    bucket.acquire()
    bucket.release()

    cancelled = threading.Event()
    cancelled.set()
    with pytest.raises(AcquireCancelledError):
        bucket.acquire(cancelled)


def test_cancellation_wakes_a_waiter_without_consuming_a_token():
    bucket = TokenBucket(rate=0.1, capacity=1)
    bucket.acquire()
    cancelled = threading.Event()
    result = []

    def wait_for_token():
        try:
            bucket.acquire(cancelled)
        except AcquireCancelledError:
            result.append("cancelled")

    thread = threading.Thread(target=wait_for_token)
    thread.start()
    time.sleep(0.03)
    cancelled.set()
    thread.join(timeout=0.5)

    assert not thread.is_alive()
    assert result == ["cancelled"]


def test_concurrent_waiters_do_not_consume_the_same_token():
    bucket = TokenBucket(rate=0.01, capacity=1)
    cancelled = threading.Event()
    start = threading.Barrier(5)
    lock = threading.Lock()
    outcomes = []

    def worker():
        start.wait()
        try:
            bucket.acquire(cancelled)
            outcome = "acquired"
        except AcquireCancelledError:
            outcome = "cancelled"
        with lock:
            outcomes.append(outcome)

    threads = [threading.Thread(target=worker) for _ in range(4)]
    for thread in threads:
        thread.start()

    start.wait()
    deadline = time.monotonic() + 0.5
    while time.monotonic() < deadline:
        with lock:
            if outcomes.count("acquired") == 1:
                break
        time.sleep(0.005)

    cancelled.set()
    for thread in threads:
        thread.join(timeout=0.5)

    assert outcomes.count("acquired") == 1
    assert outcomes.count("cancelled") == 3


class RecordingBucket(TokenBucket):
    def __init__(self):
        super().__init__(rate=1000, capacity=1)
        self.events = []

    def acquire(self, cancel_event=None):
        self.events.append("acquire")
        super().acquire(cancel_event)

    def release(self):
        self.events.append("release")
        super().release()


def test_rate_limited_decorator_acquires_and_releases():
    bucket = RecordingBucket()

    @rate_limited(bucket)
    def add(left, right):
        bucket.events.append("call")
        return left + right

    assert add(2, 3) == 5
    assert bucket.events == ["acquire", "call", "release"]
    assert add.__name__ == "add"


def test_rate_limited_decorator_releases_when_function_raises():
    bucket = RecordingBucket()

    @rate_limited(bucket)
    def fail():
        bucket.events.append("call")
        raise RuntimeError("boom")

    with pytest.raises(RuntimeError, match="boom"):
        fail()

    assert bucket.events == ["acquire", "call", "release"]


def test_rate_limited_decorator_does_not_call_or_release_when_cancelled():
    bucket = RecordingBucket()
    cancelled = threading.Event()
    cancelled.set()

    @rate_limited(bucket, cancelled)
    def should_not_run():
        bucket.events.append("call")

    with pytest.raises(AcquireCancelledError):
        should_not_run()

    assert bucket.events == ["acquire"]
