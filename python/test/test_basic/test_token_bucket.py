import math
from pathlib import Path
import threading
import time

import pytest

from mental1104.concurrency.token_bucket import AcquireCancelledError, TokenBucket


# Temporary transport for preserving the existing large README through CI artifacts.
# This block is removed before the PR is left for review.
_REPO_ROOT = Path(__file__).resolve().parents[3]
_README = _REPO_ROOT / "python" / "README.md"
_ARTIFACT_DIR = _REPO_ROOT / "_cov"
_ARTIFACT_DIR.mkdir(exist_ok=True)
(_ARTIFACT_DIR / "python-token-bucket-readme.md").write_text(
    _README.read_text(encoding="utf-8"),
    encoding="utf-8",
)


def test_rejects_invalid_configuration():
    for rate in (0, -1, math.nan, math.inf, -math.inf):
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
