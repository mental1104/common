import threading
import time
from concurrent.futures import ThreadPoolExecutor

import pytest

from mental1104.concurrency.singleflight import (
    CacheLookup,
    RebuildTimeoutError,
    RedisSingleFlight,
    RedisSingleFlightOptions,
    SingleFlightGroup,
)
from mental1104.db.redis.connection import RedisLock


def test_local_singleflight_coalesces_same_key():
    group = SingleFlightGroup()
    started = threading.Event()
    release = threading.Event()
    calls = 0
    calls_lock = threading.Lock()

    def loader():
        nonlocal calls
        with calls_lock:
            calls += 1
        started.set()
        assert release.wait(1)
        return "value"

    with ThreadPoolExecutor(max_workers=8) as executor:
        first = executor.submit(group.do, "product:123", loader)
        assert started.wait(1)
        rest = [executor.submit(group.do, "product:123", loader) for _ in range(7)]
        time.sleep(0.02)
        release.set()
        results = [first.result(timeout=1)] + [future.result(timeout=1) for future in rest]

    assert calls == 1
    assert [result.value for result in results] == ["value"] * 8
    assert sum(result.shared for result in results) == 7


def test_local_singleflight_keeps_different_keys_independent():
    group = SingleFlightGroup()
    barrier = threading.Barrier(2)

    def loader(value):
        barrier.wait(timeout=1)
        return value

    with ThreadPoolExecutor(max_workers=2) as executor:
        left = executor.submit(group.do, "left", lambda: loader(1))
        right = executor.submit(group.do, "right", lambda: loader(2))

    assert left.result().value == 1
    assert right.result().value == 2


def test_local_singleflight_shares_error_and_recovers():
    group = SingleFlightGroup()
    started = threading.Event()
    release = threading.Event()
    calls = 0

    def failing_loader():
        nonlocal calls
        calls += 1
        started.set()
        assert release.wait(1)
        raise ValueError("boom")

    with ThreadPoolExecutor(max_workers=2) as executor:
        first = executor.submit(group.do, "key", failing_loader)
        assert started.wait(1)
        second = executor.submit(group.do, "key", failing_loader)
        time.sleep(0.02)
        release.set()
        with pytest.raises(ValueError, match="boom"):
            first.result(timeout=1)
        with pytest.raises(ValueError, match="boom"):
            second.result(timeout=1)

    assert calls == 1
    assert group.do("key", lambda: "recovered").value == "recovered"


class FakeLock:
    def __init__(self, acquired):
        self.acquired = acquired
        self.try_calls = 0
        self.unlock_calls = 0

    def try_lock_once(self):
        self.try_calls += 1
        return self.acquired

    def unlock(self):
        self.unlock_calls += 1
        return 1


def test_redis_singleflight_returns_initial_cache_hit_without_locking():
    lock_created = []
    sf = RedisSingleFlight(
        object(),
        lambda key: CacheLookup.hit("cached"),
        lambda key, value, ttl: pytest.fail("cache_set must not run"),
        lock_factory=lambda client, key, ttl: lock_created.append(key),
    )

    result = sf.get_or_load("product:123", lambda: pytest.fail("loader must not run"))

    assert result.value == "cached"
    assert result.shared is False
    assert lock_created == []


def test_redis_singleflight_lock_owner_double_checks_loads_and_writes_cache():
    cache = {}
    reads = 0
    writes = []
    lock = FakeLock(acquired=True)

    def cache_get(key):
        nonlocal reads
        reads += 1
        return CacheLookup.hit(cache[key]) if key in cache else CacheLookup.miss()

    def cache_set(key, value, ttl):
        cache[key] = value
        writes.append((key, value, ttl))

    sf = RedisSingleFlight(
        object(),
        cache_get,
        cache_set,
        lock_factory=lambda client, key, ttl: lock,
    )

    result = sf.get_or_load("product:123", lambda: {"id": 123})

    assert result.value == {"id": 123}
    assert reads == 3
    assert writes == [("product:123", {"id": 123}, 600)]
    assert lock.try_calls == 1
    assert lock.unlock_calls == 1


def test_redis_singleflight_waiter_polls_until_owner_publishes():
    cache = {}
    lock = FakeLock(acquired=False)
    sleeps = 0

    def sleeper(delay):
        nonlocal sleeps
        sleeps += 1
        cache["product:123"] = "rebuilt"

    sf = RedisSingleFlight(
        object(),
        lambda key: CacheLookup.hit(cache[key]) if key in cache else CacheLookup.miss(),
        lambda key, value, ttl: pytest.fail("waiter must not write cache"),
        options=RedisSingleFlightOptions(
            wait_timeout_seconds=0.1,
            poll_min_seconds=0.001,
            poll_max_seconds=0.001,
        ),
        lock_factory=lambda client, key, ttl: lock,
        sleeper=sleeper,
        jitter=lambda low, high: low,
    )

    result = sf.get_or_load("product:123", lambda: pytest.fail("waiter must not load"))

    assert result.value == "rebuilt"
    assert sleeps == 1
    assert lock.unlock_calls == 0


def test_redis_singleflight_uses_stale_value_after_wait_timeout():
    clock = [0.0]
    lock = FakeLock(acquired=False)

    def monotonic():
        return clock[0]

    def sleeper(delay):
        clock[0] += delay

    sf = RedisSingleFlight(
        object(),
        lambda key: CacheLookup.miss(),
        lambda key, value, ttl: pytest.fail("waiter must not write cache"),
        stale_get=lambda key: CacheLookup.hit("stale"),
        options=RedisSingleFlightOptions(
            wait_timeout_seconds=0.03,
            poll_min_seconds=0.01,
            poll_max_seconds=0.01,
        ),
        lock_factory=lambda client, key, ttl: lock,
        sleeper=sleeper,
        monotonic=monotonic,
        jitter=lambda low, high: low,
    )

    result = sf.get_or_load("product:123", lambda: pytest.fail("waiter must not load"))

    assert result.value == "stale"
    assert result.stale is True


def test_redis_singleflight_raises_stable_timeout_without_stale_value():
    sf = RedisSingleFlight(
        object(),
        lambda key: CacheLookup.miss(),
        lambda key, value, ttl: None,
        options=RedisSingleFlightOptions(wait_timeout_seconds=0),
        lock_factory=lambda client, key, ttl: FakeLock(acquired=False),
    )

    with pytest.raises(RebuildTimeoutError, match="product:123"):
        sf.get_or_load("product:123", lambda: "unused")


@pytest.mark.parametrize(
    "kwargs",
    [
        {"lock_ttl_seconds": 0},
        {"cache_ttl_seconds": 0},
        {"wait_timeout_seconds": -1},
        {"poll_min_seconds": 0},
        {"poll_min_seconds": 0.1, "poll_max_seconds": 0.01},
        {"lock_prefix": ""},
    ],
)
def test_redis_singleflight_options_validate_boundaries(kwargs):
    with pytest.raises(ValueError):
        RedisSingleFlightOptions(**kwargs)


class FakeRedis:
    def __init__(self):
        self.set_calls = []
        self.script_calls = []

    def set(self, name, value, **kwargs):
        self.set_calls.append((name, value, kwargs))
        return True

    def register_script(self, script):
        def execute(*, keys, args):
            self.script_calls.append((keys, args))
            return 1

        return execute


def test_redis_lock_exposes_non_blocking_single_attempt():
    redis_client = FakeRedis()
    lock = RedisLock(redis_client, "lock:key", lock_expire=3)

    assert lock.try_lock_once() is True
    assert redis_client.set_calls[0][0] == "lock:key"
    assert redis_client.set_calls[0][2] == {"nx": True, "ex": 3}
    assert lock.unlock() == 1
    assert redis_client.script_calls == [(["lock:key"], [lock.lock_value])]
