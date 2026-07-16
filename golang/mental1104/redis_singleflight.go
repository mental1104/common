package mental1104

import (
	"context"
	"errors"
	"math/rand"
	"time"

	redis "github.com/redis/go-redis/v9"
)

var ErrRebuildTimeout = errors.New("singleflight: cache rebuild timed out")

type CacheLookup[V any] struct {
	Value V
	Found bool
}

func CacheHit[V any](value V) CacheLookup[V] {
	return CacheLookup[V]{Value: value, Found: true}
}

func CacheMiss[V any]() CacheLookup[V] {
	return CacheLookup[V]{}
}

type RedisSingleFlightResult[V any] struct {
	Value  V
	Shared bool
	Stale  bool
}

type RedisSingleFlightOptions struct {
	LockTTL     time.Duration
	CacheTTL    time.Duration
	WaitTimeout time.Duration
	PollMin     time.Duration
	PollMax     time.Duration
	LockPrefix  string
}

func DefaultRedisSingleFlightOptions() RedisSingleFlightOptions {
	return RedisSingleFlightOptions{
		LockTTL:     3 * time.Second,
		CacheTTL:    10 * time.Minute,
		WaitTimeout: 500 * time.Millisecond,
		PollMin:     20 * time.Millisecond,
		PollMax:     50 * time.Millisecond,
		LockPrefix:  "singleflight:lock:",
	}
}

func (o RedisSingleFlightOptions) validate() error {
	if o.LockTTL <= 0 {
		return errors.New("singleflight: lock ttl must be positive")
	}
	if o.CacheTTL <= 0 {
		return errors.New("singleflight: cache ttl must be positive")
	}
	if o.WaitTimeout < 0 {
		return errors.New("singleflight: wait timeout must not be negative")
	}
	if o.PollMin <= 0 {
		return errors.New("singleflight: poll minimum must be positive")
	}
	if o.PollMax < o.PollMin {
		return errors.New("singleflight: poll maximum must not be less than poll minimum")
	}
	if o.LockPrefix == "" {
		return errors.New("singleflight: lock prefix must not be empty")
	}
	return nil
}

type RedisCacheGet[V any] func(context.Context, string) (CacheLookup[V], error)
type RedisCacheSet[V any] func(context.Context, string, V, time.Duration) error

type redisTryLocker interface {
	TryLock(context.Context) (bool, error)
	Unlock(context.Context) (bool, error)
}

type redisLockFactory func(string, time.Duration) (redisTryLocker, error)

type coordinatedValue[V any] struct {
	value V
	stale bool
}

type RedisSingleFlight[V any] struct {
	local       SingleFlightGroup[string, coordinatedValue[V]]
	cacheGet    RedisCacheGet[V]
	cacheSet    RedisCacheSet[V]
	staleGet    RedisCacheGet[V]
	options     RedisSingleFlightOptions
	newLock     redisLockFactory
	pollJitter  func(time.Duration, time.Duration) time.Duration
}

func NewRedisSingleFlight[V any](
	client redis.UniversalClient,
	cacheGet RedisCacheGet[V],
	cacheSet RedisCacheSet[V],
	staleGet RedisCacheGet[V],
	options RedisSingleFlightOptions,
) (*RedisSingleFlight[V], error) {
	if client == nil {
		return nil, errors.New("singleflight: Redis client must not be nil")
	}
	return newRedisSingleFlight(
		cacheGet,
		cacheSet,
		staleGet,
		options,
		func(key string, ttl time.Duration) (redisTryLocker, error) {
			return NewRedisLock(client, key, ttl)
		},
	)
}

func newRedisSingleFlight[V any](
	cacheGet RedisCacheGet[V],
	cacheSet RedisCacheSet[V],
	staleGet RedisCacheGet[V],
	options RedisSingleFlightOptions,
	newLock redisLockFactory,
) (*RedisSingleFlight[V], error) {
	if cacheGet == nil {
		return nil, errors.New("singleflight: cache get callback must not be nil")
	}
	if cacheSet == nil {
		return nil, errors.New("singleflight: cache set callback must not be nil")
	}
	if newLock == nil {
		return nil, errors.New("singleflight: lock factory must not be nil")
	}
	if err := options.validate(); err != nil {
		return nil, err
	}
	return &RedisSingleFlight[V]{
		cacheGet:   cacheGet,
		cacheSet:   cacheSet,
		staleGet:   staleGet,
		options:    options,
		newLock:    newLock,
		pollJitter: randomPollDuration,
	}, nil
}

func (s *RedisSingleFlight[V]) GetOrLoad(
	ctx context.Context,
	key string,
	loader func(context.Context) (V, error),
) (RedisSingleFlightResult[V], error) {
	if ctx == nil {
		return RedisSingleFlightResult[V]{}, ErrSingleFlightNilContext
	}
	if key == "" {
		return RedisSingleFlightResult[V]{}, errors.New("singleflight: key must not be empty")
	}
	if loader == nil {
		return RedisSingleFlightResult[V]{}, errors.New("singleflight: loader must not be nil")
	}

	cached, err := s.cacheGet(ctx, key)
	if err != nil {
		return RedisSingleFlightResult[V]{}, err
	}
	if cached.Found {
		return RedisSingleFlightResult[V]{Value: cached.Value}, nil
	}

	localResult, err := s.local.Do(ctx, key, func(leaderCtx context.Context) (coordinatedValue[V], error) {
		return s.coordinate(leaderCtx, key, loader)
	})
	if err != nil {
		return RedisSingleFlightResult[V]{}, err
	}
	return RedisSingleFlightResult[V]{
		Value:  localResult.Value.value,
		Shared: localResult.Shared,
		Stale:  localResult.Value.stale,
	}, nil
}

func (s *RedisSingleFlight[V]) coordinate(
	ctx context.Context,
	key string,
	loader func(context.Context) (V, error),
) (result coordinatedValue[V], err error) {
	cached, err := s.cacheGet(ctx, key)
	if err != nil {
		return result, err
	}
	if cached.Found {
		return coordinatedValue[V]{value: cached.Value}, nil
	}

	lock, err := s.newLock(s.options.LockPrefix+key, s.options.LockTTL)
	if err != nil {
		return result, err
	}
	locked, err := lock.TryLock(ctx)
	if err != nil {
		return result, err
	}
	if locked {
		defer func() {
			unlockTimeout := s.options.LockTTL
			if unlockTimeout > time.Second {
				unlockTimeout = time.Second
			}
			unlockCtx, cancel := context.WithTimeout(context.Background(), unlockTimeout)
			defer cancel()
			_, unlockErr := lock.Unlock(unlockCtx)
			if unlockErr != nil {
				err = errors.Join(err, unlockErr)
			}
		}()

		cached, err = s.cacheGet(ctx, key)
		if err != nil {
			return result, err
		}
		if cached.Found {
			return coordinatedValue[V]{value: cached.Value}, nil
		}

		value, loadErr := loader(ctx)
		if loadErr != nil {
			return result, loadErr
		}
		if err = s.cacheSet(ctx, key, value, s.options.CacheTTL); err != nil {
			return result, err
		}
		return coordinatedValue[V]{value: value}, nil
	}

	deadline := time.Now().Add(s.options.WaitTimeout)
	for {
		remaining := time.Until(deadline)
		if remaining <= 0 {
			break
		}
		delay := s.pollJitter(s.options.PollMin, s.options.PollMax)
		if delay > remaining {
			delay = remaining
		}

		timer := time.NewTimer(delay)
		select {
		case <-timer.C:
		case <-ctx.Done():
			if !timer.Stop() {
				select {
				case <-timer.C:
				default:
				}
			}
			return result, ctx.Err()
		}

		cached, err = s.cacheGet(ctx, key)
		if err != nil {
			return result, err
		}
		if cached.Found {
			return coordinatedValue[V]{value: cached.Value}, nil
		}
	}

	if s.staleGet != nil {
		stale, staleErr := s.staleGet(ctx, key)
		if staleErr != nil {
			return result, staleErr
		}
		if stale.Found {
			return coordinatedValue[V]{value: stale.Value, stale: true}, nil
		}
	}
	return result, ErrRebuildTimeout
}

func randomPollDuration(minimum, maximum time.Duration) time.Duration {
	if minimum == maximum {
		return minimum
	}
	return minimum + time.Duration(rand.Int63n(int64(maximum-minimum)+1))
}
