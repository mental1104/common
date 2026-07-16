from __future__ import annotations

import random
import threading
import time
from dataclasses import dataclass
from typing import Callable, Dict, Generic, Optional, TypeVar

from mental1104.db.redis.connection import RedisLock


K = TypeVar("K")
T = TypeVar("T")


class RebuildTimeoutError(TimeoutError):
    """Raised when another instance does not publish the rebuilt value in time."""


@dataclass(frozen=True)
class CacheLookup(Generic[T]):
    """A cache lookup result that can represent cached ``None`` values safely."""

    found: bool
    value: Optional[T] = None

    @classmethod
    def hit(cls, value: T) -> "CacheLookup[T]":
        return cls(found=True, value=value)

    @classmethod
    def miss(cls) -> "CacheLookup[T]":
        return cls(found=False)


@dataclass(frozen=True)
class SingleFlightResult(Generic[T]):
    value: T
    shared: bool
    stale: bool = False


class _Call(Generic[T]):
    def __init__(self) -> None:
        self.done = threading.Event()
        self.value: Optional[T] = None
        self.error: Optional[BaseException] = None


class SingleFlightGroup(Generic[K, T]):
    """Coalesce concurrent calls with the same key inside one process."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._calls: Dict[K, _Call[T]] = {}

    def do(self, key: K, loader: Callable[[], T]) -> SingleFlightResult[T]:
        with self._lock:
            call = self._calls.get(key)
            if call is None:
                call = _Call()
                self._calls[key] = call
                leader = True
            else:
                leader = False

        if not leader:
            call.done.wait()
            if call.error is not None:
                raise call.error
            return SingleFlightResult(
                value=call.value,
                shared=True,
            )  # type: ignore[arg-type]

        try:
            call.value = loader()
        except BaseException as exc:
            call.error = exc
        finally:
            call.done.set()
            with self._lock:
                if self._calls.get(key) is call:
                    del self._calls[key]

        if call.error is not None:
            raise call.error
        return SingleFlightResult(
            value=call.value,
            shared=False,
        )  # type: ignore[arg-type]


@dataclass(frozen=True)
class RedisSingleFlightOptions:
    lock_ttl_seconds: int = 3
    cache_ttl_seconds: int = 600
    wait_timeout_seconds: float = 0.5
    poll_min_seconds: float = 0.02
    poll_max_seconds: float = 0.05
    lock_prefix: str = "singleflight:lock:"

    def __post_init__(self) -> None:
        if self.lock_ttl_seconds <= 0:
            raise ValueError("lock_ttl_seconds must be positive")
        if self.cache_ttl_seconds <= 0:
            raise ValueError("cache_ttl_seconds must be positive")
        if self.wait_timeout_seconds < 0:
            raise ValueError("wait_timeout_seconds must not be negative")
        if self.poll_min_seconds <= 0:
            raise ValueError("poll_min_seconds must be positive")
        if self.poll_max_seconds < self.poll_min_seconds:
            raise ValueError(
                "poll_max_seconds must be greater than or equal to poll_min_seconds"
            )
        if not self.lock_prefix:
            raise ValueError("lock_prefix must not be empty")


@dataclass(frozen=True)
class _CoordinatedValue(Generic[T]):
    value: T
    stale: bool = False


class RedisSingleFlight(Generic[T]):
    """Combine local singleflight, Redis locking, polling, and stale fallback."""

    def __init__(
        self,
        redis_client,
        cache_get: Callable[[str], CacheLookup[T]],
        cache_set: Callable[[str, T, int], None],
        *,
        stale_get: Optional[Callable[[str], CacheLookup[T]]] = None,
        options: Optional[RedisSingleFlightOptions] = None,
        lock_factory: Optional[Callable[[object, str, int], RedisLock]] = None,
        sleeper: Callable[[float], None] = time.sleep,
        monotonic: Callable[[], float] = time.monotonic,
        jitter: Callable[[float, float], float] = random.uniform,
    ) -> None:
        self._redis_client = redis_client
        self._cache_get = cache_get
        self._cache_set = cache_set
        self._stale_get = stale_get
        self._options = options or RedisSingleFlightOptions()
        self._lock_factory = lock_factory or (
            lambda client, key, ttl: RedisLock(client, key, lock_expire=ttl)
        )
        self._sleeper = sleeper
        self._monotonic = monotonic
        self._jitter = jitter
        self._local = SingleFlightGroup[str, _CoordinatedValue[T]]()

    def get_or_load(
        self,
        key: str,
        loader: Callable[[], T],
    ) -> SingleFlightResult[T]:
        if not key:
            raise ValueError("key must not be empty")

        cached = self._cache_get(key)
        if cached.found:
            return SingleFlightResult(
                value=cached.value,
                shared=False,
            )  # type: ignore[arg-type]

        local_result = self._local.do(key, lambda: self._coordinate(key, loader))
        return SingleFlightResult(
            value=local_result.value.value,
            shared=local_result.shared,
            stale=local_result.value.stale,
        )

    def _coordinate(
        self,
        key: str,
        loader: Callable[[], T],
    ) -> _CoordinatedValue[T]:
        cached = self._cache_get(key)
        if cached.found:
            return _CoordinatedValue(cached.value)  # type: ignore[arg-type]

        lock_key = self._options.lock_prefix + key
        lock = self._lock_factory(
            self._redis_client,
            lock_key,
            self._options.lock_ttl_seconds,
        )

        if lock.try_lock_once():
            try:
                cached = self._cache_get(key)
                if cached.found:
                    return _CoordinatedValue(
                        cached.value
                    )  # type: ignore[arg-type]

                value = loader()
                self._cache_set(key, value, self._options.cache_ttl_seconds)
                return _CoordinatedValue(value)
            finally:
                lock.unlock()

        deadline = self._monotonic() + self._options.wait_timeout_seconds
        while True:
            remaining = deadline - self._monotonic()
            if remaining <= 0:
                break

            delay = self._jitter(
                self._options.poll_min_seconds,
                self._options.poll_max_seconds,
            )
            self._sleeper(min(delay, remaining))

            cached = self._cache_get(key)
            if cached.found:
                return _CoordinatedValue(cached.value)  # type: ignore[arg-type]

        if self._stale_get is not None:
            stale = self._stale_get(key)
            if stale.found:
                return _CoordinatedValue(
                    stale.value,
                    stale=True,
                )  # type: ignore[arg-type]

        raise RebuildTimeoutError(
            "singleflight cache rebuild timed out for key: %s" % key
        )
