package mental1104

import (
	"context"
	"errors"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

func TestSingleFlightCoalescesSameKey(t *testing.T) {
	var group SingleFlightGroup[string, string]
	started := make(chan struct{})
	release := make(chan struct{})
	var calls atomic.Int32

	loader := func(context.Context) (string, error) {
		if calls.Add(1) == 1 {
			close(started)
		}
		<-release
		return "value", nil
	}

	const workers = 8
	results := make(chan SingleFlightResult[string], workers)
	errs := make(chan error, workers)
	go func() {
		result, err := group.Do(context.Background(), "product:123", loader)
		results <- result
		errs <- err
	}()
	<-started
	for i := 1; i < workers; i++ {
		go func() {
			result, err := group.Do(context.Background(), "product:123", loader)
			results <- result
			errs <- err
		}()
	}
	time.Sleep(20 * time.Millisecond)
	close(release)

	shared := 0
	for i := 0; i < workers; i++ {
		if err := <-errs; err != nil {
			t.Fatal(err)
		}
		result := <-results
		if result.Value != "value" {
			t.Fatalf("unexpected value: %q", result.Value)
		}
		if result.Shared {
			shared++
		}
	}
	if calls.Load() != 1 {
		t.Fatalf("loader calls = %d, want 1", calls.Load())
	}
	if shared != workers-1 {
		t.Fatalf("shared results = %d, want %d", shared, workers-1)
	}
}

func TestSingleFlightDifferentKeysRunIndependently(t *testing.T) {
	var group SingleFlightGroup[string, int]
	ready := make(chan struct{}, 2)
	release := make(chan struct{})
	var wg sync.WaitGroup
	wg.Add(2)

	for _, item := range []struct {
		key   string
		value int
	}{{"left", 1}, {"right", 2}} {
		item := item
		go func() {
			defer wg.Done()
			result, err := group.Do(context.Background(), item.key, func(context.Context) (int, error) {
				ready <- struct{}{}
				<-release
				return item.value, nil
			})
			if err != nil || result.Value != item.value {
				t.Errorf("key %s: result=%+v err=%v", item.key, result, err)
			}
		}()
	}
	<-ready
	<-ready
	close(release)
	wg.Wait()
}

func TestSingleFlightWaiterCanCancelWithoutStoppingLeader(t *testing.T) {
	var group SingleFlightGroup[string, string]
	started := make(chan struct{})
	release := make(chan struct{})
	leaderDone := make(chan error, 1)

	go func() {
		_, err := group.Do(context.Background(), "key", func(context.Context) (string, error) {
			close(started)
			<-release
			return "ok", nil
		})
		leaderDone <- err
	}()
	<-started

	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := group.Do(ctx, "key", func(context.Context) (string, error) {
		t.Fatal("waiter must not run loader")
		return "", nil
	}); !errors.Is(err, context.Canceled) {
		t.Fatalf("got %v, want context.Canceled", err)
	}

	close(release)
	if err := <-leaderDone; err != nil {
		t.Fatal(err)
	}
}

func TestSingleFlightSharesErrorAndRecovers(t *testing.T) {
	var group SingleFlightGroup[string, string]
	started := make(chan struct{})
	release := make(chan struct{})
	var calls atomic.Int32
	loader := func(context.Context) (string, error) {
		if calls.Add(1) == 1 {
			close(started)
		}
		<-release
		return "", errors.New("boom")
	}

	first := make(chan error, 1)
	second := make(chan error, 1)
	go func() {
		_, err := group.Do(context.Background(), "key", loader)
		first <- err
	}()
	<-started
	go func() {
		_, err := group.Do(context.Background(), "key", loader)
		second <- err
	}()
	time.Sleep(20 * time.Millisecond)
	close(release)
	if err := <-first; err == nil || err.Error() != "boom" {
		t.Fatalf("first error = %v", err)
	}
	if err := <-second; err == nil || err.Error() != "boom" {
		t.Fatalf("second error = %v", err)
	}
	if calls.Load() != 1 {
		t.Fatalf("loader calls = %d, want 1", calls.Load())
	}

	result, err := group.Do(context.Background(), "key", func(context.Context) (string, error) {
		return "recovered", nil
	})
	if err != nil || result.Value != "recovered" {
		t.Fatalf("result=%+v err=%v", result, err)
	}
}

type fakeRedisCommands struct {
	setResult  bool
	setErr     error
	evalResult int64
	evalErr    error
	setKey     string
	setValue   string
	setTTL     time.Duration
	evalScript string
	evalKeys   []string
	evalArgs   []any
}

func (f *fakeRedisCommands) setNX(
	_ context.Context,
	key string,
	value string,
	ttl time.Duration,
) (bool, error) {
	f.setKey = key
	f.setValue = value
	f.setTTL = ttl
	return f.setResult, f.setErr
}

func (f *fakeRedisCommands) evalInt(
	_ context.Context,
	script string,
	keys []string,
	args ...any,
) (int64, error) {
	f.evalScript = script
	f.evalKeys = append([]string(nil), keys...)
	f.evalArgs = append([]any(nil), args...)
	return f.evalResult, f.evalErr
}

func TestRedisLockUsesSetNXAndOwnerCheckedDelete(t *testing.T) {
	commands := &fakeRedisCommands{setResult: true, evalResult: 1}
	lock, err := newRedisLock(commands, "singleflight:lock:key", 3*time.Second)
	if err != nil {
		t.Fatal(err)
	}

	locked, err := lock.TryLock(context.Background())
	if err != nil || !locked {
		t.Fatalf("locked=%v err=%v", locked, err)
	}
	if commands.setKey != "singleflight:lock:key" || commands.setTTL != 3*time.Second {
		t.Fatalf("unexpected SET NX args: key=%q ttl=%v", commands.setKey, commands.setTTL)
	}
	if len(commands.setValue) != 32 {
		t.Fatalf("token length = %d, want 32", len(commands.setValue))
	}

	unlocked, err := lock.Unlock(context.Background())
	if err != nil || !unlocked {
		t.Fatalf("unlocked=%v err=%v", unlocked, err)
	}
	if !strings.Contains(commands.evalScript, `redis.call("get"`) ||
		!strings.Contains(commands.evalScript, `redis.call("del"`) {
		t.Fatalf("unlock script is not owner checked: %q", commands.evalScript)
	}
	if len(commands.evalKeys) != 1 || commands.evalKeys[0] != "singleflight:lock:key" {
		t.Fatalf("unexpected eval keys: %#v", commands.evalKeys)
	}
	if len(commands.evalArgs) != 1 || commands.evalArgs[0] != commands.setValue {
		t.Fatalf("unexpected eval args: %#v", commands.evalArgs)
	}
}

func TestRedisLockValidatesInputs(t *testing.T) {
	commands := &fakeRedisCommands{}
	for _, test := range []struct {
		key string
		ttl time.Duration
	}{{"", time.Second}, {"key", 0}, {"key", -time.Second}} {
		if _, err := newRedisLock(commands, test.key, test.ttl); err == nil {
			t.Fatalf("key=%q ttl=%v should fail", test.key, test.ttl)
		}
	}
	lock, err := newRedisLock(commands, "key", time.Second)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := lock.TryLock(nil); !errors.Is(err, ErrRedisLockNilContext) {
		t.Fatalf("got %v, want ErrRedisLockNilContext", err)
	}
}

type fakeTryLocker struct {
	acquired    bool
	tryErr      error
	unlockErr   error
	tryCalls    atomic.Int32
	unlockCalls atomic.Int32
}

func (l *fakeTryLocker) TryLock(context.Context) (bool, error) {
	l.tryCalls.Add(1)
	return l.acquired, l.tryErr
}

func (l *fakeTryLocker) Unlock(context.Context) (bool, error) {
	l.unlockCalls.Add(1)
	return true, l.unlockErr
}

func TestRedisSingleFlightOwnerLoadsAndWritesCache(t *testing.T) {
	var mu sync.Mutex
	cache := map[string]string{}
	reads := 0
	writes := 0
	lock := &fakeTryLocker{acquired: true}
	options := DefaultRedisSingleFlightOptions()

	sf, err := newRedisSingleFlight(
		func(_ context.Context, key string) (CacheLookup[string], error) {
			mu.Lock()
			defer mu.Unlock()
			reads++
			value, ok := cache[key]
			if !ok {
				return CacheMiss[string](), nil
			}
			return CacheHit(value), nil
		},
		func(_ context.Context, key, value string, ttl time.Duration) error {
			mu.Lock()
			defer mu.Unlock()
			writes++
			if ttl != options.CacheTTL {
				t.Fatalf("cache ttl = %v, want %v", ttl, options.CacheTTL)
			}
			cache[key] = value
			return nil
		},
		nil,
		options,
		func(key string, ttl time.Duration) (redisTryLocker, error) {
			if key != options.LockPrefix+"product:123" || ttl != options.LockTTL {
				t.Fatalf("unexpected lock args: key=%q ttl=%v", key, ttl)
			}
			return lock, nil
		},
	)
	if err != nil {
		t.Fatal(err)
	}

	result, err := sf.GetOrLoad(context.Background(), "product:123", func(context.Context) (string, error) {
		return "rebuilt", nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if result.Value != "rebuilt" || result.Stale || result.Shared {
		t.Fatalf("unexpected result: %+v", result)
	}
	if reads != 3 || writes != 1 {
		t.Fatalf("reads=%d writes=%d, want 3/1", reads, writes)
	}
	if lock.tryCalls.Load() != 1 || lock.unlockCalls.Load() != 1 {
		t.Fatalf("try=%d unlock=%d", lock.tryCalls.Load(), lock.unlockCalls.Load())
	}
}

func TestRedisSingleFlightWaiterPollsForPublishedValue(t *testing.T) {
	var reads atomic.Int32
	lock := &fakeTryLocker{acquired: false}
	options := DefaultRedisSingleFlightOptions()
	options.WaitTimeout = 100 * time.Millisecond
	options.PollMin = time.Millisecond
	options.PollMax = time.Millisecond

	sf, err := newRedisSingleFlight(
		func(context.Context, string) (CacheLookup[string], error) {
			if reads.Add(1) >= 3 {
				return CacheHit("rebuilt"), nil
			}
			return CacheMiss[string](), nil
		},
		func(context.Context, string, string, time.Duration) error {
			t.Fatal("waiter must not write cache")
			return nil
		},
		nil,
		options,
		func(string, time.Duration) (redisTryLocker, error) { return lock, nil },
	)
	if err != nil {
		t.Fatal(err)
	}
	sf.pollJitter = func(time.Duration, time.Duration) time.Duration { return time.Millisecond }

	result, err := sf.GetOrLoad(context.Background(), "product:123", func(context.Context) (string, error) {
		t.Fatal("waiter must not call loader")
		return "", nil
	})
	if err != nil || result.Value != "rebuilt" {
		t.Fatalf("result=%+v err=%v", result, err)
	}
	if lock.unlockCalls.Load() != 0 {
		t.Fatalf("waiter unlock calls = %d", lock.unlockCalls.Load())
	}
}

func TestRedisSingleFlightReturnsStaleAfterTimeout(t *testing.T) {
	options := DefaultRedisSingleFlightOptions()
	options.WaitTimeout = 0
	lock := &fakeTryLocker{acquired: false}

	sf, err := newRedisSingleFlight(
		func(context.Context, string) (CacheLookup[string], error) {
			return CacheMiss[string](), nil
		},
		func(context.Context, string, string, time.Duration) error { return nil },
		func(context.Context, string) (CacheLookup[string], error) {
			return CacheHit("stale"), nil
		},
		options,
		func(string, time.Duration) (redisTryLocker, error) { return lock, nil },
	)
	if err != nil {
		t.Fatal(err)
	}

	result, err := sf.GetOrLoad(context.Background(), "product:123", func(context.Context) (string, error) {
		return "unused", nil
	})
	if err != nil || result.Value != "stale" || !result.Stale {
		t.Fatalf("result=%+v err=%v", result, err)
	}
}

func TestRedisSingleFlightReturnsStableTimeout(t *testing.T) {
	options := DefaultRedisSingleFlightOptions()
	options.WaitTimeout = 0
	sf, err := newRedisSingleFlight(
		func(context.Context, string) (CacheLookup[string], error) {
			return CacheMiss[string](), nil
		},
		func(context.Context, string, string, time.Duration) error { return nil },
		nil,
		options,
		func(string, time.Duration) (redisTryLocker, error) {
			return &fakeTryLocker{acquired: false}, nil
		},
	)
	if err != nil {
		t.Fatal(err)
	}

	_, err = sf.GetOrLoad(context.Background(), "product:123", func(context.Context) (string, error) {
		return "unused", nil
	})
	if !errors.Is(err, ErrRebuildTimeout) {
		t.Fatalf("got %v, want ErrRebuildTimeout", err)
	}
}

func TestRedisSingleFlightOptionsValidateBoundaries(t *testing.T) {
	valid := DefaultRedisSingleFlightOptions()
	cases := []RedisSingleFlightOptions{
		func() RedisSingleFlightOptions { v := valid; v.LockTTL = 0; return v }(),
		func() RedisSingleFlightOptions { v := valid; v.CacheTTL = 0; return v }(),
		func() RedisSingleFlightOptions { v := valid; v.WaitTimeout = -1; return v }(),
		func() RedisSingleFlightOptions { v := valid; v.PollMin = 0; return v }(),
		func() RedisSingleFlightOptions { v := valid; v.PollMax = v.PollMin - 1; return v }(),
		func() RedisSingleFlightOptions { v := valid; v.LockPrefix = ""; return v }(),
	}
	for index, options := range cases {
		if err := options.validate(); err == nil {
			t.Fatalf("case %d should fail", index)
		}
	}
}
