package mental1104

import (
	"errors"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

type manualClock struct {
	mu  sync.Mutex
	now time.Time
}

func newManualClock() *manualClock {
	return &manualClock{now: time.Unix(0, 0)}
}

func (c *manualClock) Now() time.Time {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.now
}

func (c *manualClock) Advance(duration time.Duration) {
	c.mu.Lock()
	c.now = c.now.Add(duration)
	c.mu.Unlock()
}

func testCircuitConfig() CircuitBreakerConfig {
	return CircuitBreakerConfig{
		Window:                   10 * time.Second,
		MinimumRequests:          3,
		FailureRateThreshold:     0.5,
		SlowCallDuration:         time.Second,
		SlowCallRateThreshold:    0.5,
		OpenDuration:             5 * time.Second,
		HalfOpenMaxProbes:        3,
		HalfOpenSuccessesToClose: 3,
	}
}

func newTestBreaker(t *testing.T, clock *manualClock, options ...CircuitBreakerOption) *CircuitBreaker {
	t.Helper()
	options = append(options, withCircuitBreakerClock(clock.Now))
	breaker, err := NewCircuitBreaker(testCircuitConfig(), options...)
	if err != nil {
		t.Fatal(err)
	}
	return breaker
}

func openTestBreaker(t *testing.T, breaker *CircuitBreaker) {
	t.Helper()
	for i := 0; i < 3; i++ {
		permit, err := breaker.TryAcquire()
		if err != nil {
			t.Fatal(err)
		}
		permit.RecordFailure()
	}
	if breaker.State() != CircuitOpen {
		t.Fatalf("expected open, got %s", breaker.State())
	}
}

func TestCircuitBreakerConfigValidation(t *testing.T) {
	base := testCircuitConfig()
	cases := []CircuitBreakerConfig{
		func() CircuitBreakerConfig { c := base; c.Window = 0; return c }(),
		func() CircuitBreakerConfig { c := base; c.MinimumRequests = 0; return c }(),
		func() CircuitBreakerConfig { c := base; c.FailureRateThreshold = -0.1; return c }(),
		func() CircuitBreakerConfig { c := base; c.FailureRateThreshold = 1.1; return c }(),
		func() CircuitBreakerConfig { c := base; c.SlowCallDuration = 0; return c }(),
		func() CircuitBreakerConfig { c := base; c.SlowCallRateThreshold = -0.1; return c }(),
		func() CircuitBreakerConfig { c := base; c.SlowCallRateThreshold = 1.1; return c }(),
		func() CircuitBreakerConfig { c := base; c.OpenDuration = 0; return c }(),
		func() CircuitBreakerConfig { c := base; c.HalfOpenMaxProbes = 0; return c }(),
		func() CircuitBreakerConfig { c := base; c.HalfOpenSuccessesToClose = 0; return c }(),
		func() CircuitBreakerConfig { c := base; c.HalfOpenMaxProbes = 2; return c }(),
	}
	for index, config := range cases {
		if _, err := NewCircuitBreaker(config); err == nil {
			t.Fatalf("case %d should fail", index)
		}
	}
}

func TestMinimumRequestsAndFailureRateOpenCircuit(t *testing.T) {
	clock := newManualClock()
	breaker := newTestBreaker(t, clock)

	permit, _ := breaker.TryAcquire()
	permit.RecordFailure()
	permit, _ = breaker.TryAcquire()
	permit.RecordSuccess()
	if breaker.State() != CircuitClosed {
		t.Fatal("minimum request guard should keep circuit closed")
	}

	permit, _ = breaker.TryAcquire()
	permit.RecordFailure()
	if breaker.State() != CircuitOpen {
		t.Fatal("failure rate should open circuit")
	}
}

func TestSlowSuccessesCanOpenCircuit(t *testing.T) {
	clock := newManualClock()
	config := testCircuitConfig()
	config.MinimumRequests = 2
	config.FailureRateThreshold = 1
	breaker, err := NewCircuitBreaker(config, withCircuitBreakerClock(clock.Now))
	if err != nil {
		t.Fatal(err)
	}

	permit, _ := breaker.TryAcquire()
	permit.RecordSuccess()
	permit, _ = breaker.TryAcquire()
	clock.Advance(time.Second)
	permit.RecordSuccess()
	if breaker.State() != CircuitOpen {
		t.Fatal("slow-call rate should open circuit")
	}
}

func TestIgnoredErrorsAreExcludedFromClosedWindow(t *testing.T) {
	clock := newManualClock()
	breaker := newTestBreaker(t, clock)
	permit, _ := breaker.TryAcquire()
	permit.RecordIgnored()
	permit, _ = breaker.TryAcquire()
	permit.RecordFailure()

	snapshot := breaker.Snapshot()
	if snapshot.WindowRequests != 1 || snapshot.WindowFailures != 1 {
		t.Fatalf("unexpected snapshot: %+v", snapshot)
	}
	if snapshot.State != CircuitClosed {
		t.Fatal("ignored result must not satisfy minimum requests")
	}
}

func TestSlidingWindowPrunesExpiredEvents(t *testing.T) {
	clock := newManualClock()
	config := testCircuitConfig()
	config.Window = 2 * time.Second
	breaker, err := NewCircuitBreaker(config, withCircuitBreakerClock(clock.Now))
	if err != nil {
		t.Fatal(err)
	}
	permit, _ := breaker.TryAcquire()
	permit.RecordFailure()
	clock.Advance(2100 * time.Millisecond)
	permit, _ = breaker.TryAcquire()
	permit.RecordSuccess()

	snapshot := breaker.Snapshot()
	if snapshot.WindowRequests != 1 || snapshot.WindowFailures != 0 {
		t.Fatalf("expired event was not pruned: %+v", snapshot)
	}
}

func TestOpenRejectsOperationAndFallbackRuns(t *testing.T) {
	clock := newManualClock()
	breaker := newTestBreaker(t, clock)
	openTestBreaker(t, breaker)
	var called atomic.Int32

	operation := func() (string, error) {
		called.Add(1)
		return "downstream", nil
	}
	if _, err := Execute(breaker, operation, nil); !errors.Is(err, ErrCircuitOpen) {
		t.Fatalf("expected open error, got %v", err)
	}
	value, err := ExecuteOrFallback(breaker, operation, nil, func(*CircuitOpenError) (string, error) {
		return "cached", nil
	})
	if err != nil || value != "cached" || called.Load() != 0 {
		t.Fatalf("unexpected fallback result value=%q err=%v called=%d", value, err, called.Load())
	}
}

func TestHalfOpenFixedProbeRoundClosesAfterHealthyResults(t *testing.T) {
	clock := newManualClock()
	breaker := newTestBreaker(t, clock)
	openTestBreaker(t, breaker)
	clock.Advance(5 * time.Second)

	permits := make([]*CircuitPermit, 0, 3)
	for i := 0; i < 3; i++ {
		permit, err := breaker.TryAcquire()
		if err != nil {
			t.Fatal(err)
		}
		permits = append(permits, permit)
	}
	if _, err := breaker.TryAcquire(); !errors.Is(err, ErrCircuitOpen) {
		t.Fatalf("fourth probe should be rejected: %v", err)
	}
	permits[0].RecordSuccess()
	permits[1].RecordIgnored()
	permits[2].RecordSuccess()
	if breaker.State() != CircuitClosed {
		t.Fatalf("healthy probe round should close circuit, got %s", breaker.State())
	}
}

func TestHalfOpenFailureOrSlowCallReopens(t *testing.T) {
	clock := newManualClock()
	breaker := newTestBreaker(t, clock)
	openTestBreaker(t, breaker)
	clock.Advance(5 * time.Second)

	permit, _ := breaker.TryAcquire()
	permit.RecordFailure()
	if breaker.State() != CircuitOpen {
		t.Fatal("failed probe should reopen")
	}

	clock.Advance(5 * time.Second)
	permit, _ = breaker.TryAcquire()
	clock.Advance(time.Second)
	permit.RecordSuccess()
	if breaker.State() != CircuitOpen {
		t.Fatal("slow probe should reopen")
	}
}

func TestStaleCompletionAndDuplicateCompletionAreIgnored(t *testing.T) {
	clock := newManualClock()
	breaker := newTestBreaker(t, clock)
	stale, _ := breaker.TryAcquire()
	openTestBreaker(t, breaker)
	if !stale.RecordSuccess() {
		t.Fatal("first completion should be accepted by permit")
	}
	if stale.RecordFailure() {
		t.Fatal("duplicate completion should be rejected")
	}
	if breaker.Snapshot().WindowRequests != 0 {
		t.Fatal("stale result must not enter new generation")
	}
}

func TestConcurrentHalfOpenAcquisitionRespectsLimit(t *testing.T) {
	clock := newManualClock()
	breaker := newTestBreaker(t, clock)
	openTestBreaker(t, breaker)
	clock.Advance(5 * time.Second)

	start := make(chan struct{})
	permits := make(chan *CircuitPermit, 10)
	rejections := make(chan error, 10)
	var ready sync.WaitGroup
	ready.Add(10)
	for i := 0; i < 10; i++ {
		go func() {
			ready.Done()
			<-start
			permit, err := breaker.TryAcquire()
			if err != nil {
				rejections <- err
				return
			}
			permits <- permit
		}()
	}
	ready.Wait()
	close(start)

	collected := make([]*CircuitPermit, 0, 3)
	for i := 0; i < 10; i++ {
		select {
		case permit := <-permits:
			collected = append(collected, permit)
		case err := <-rejections:
			if !errors.Is(err, ErrCircuitOpen) {
				t.Fatalf("unexpected error: %v", err)
			}
		case <-time.After(time.Second):
			t.Fatal("worker did not finish")
		}
	}
	if len(collected) != 3 {
		t.Fatalf("expected 3 permits, got %d", len(collected))
	}
	for _, permit := range collected {
		permit.RecordSuccess()
	}
	if breaker.State() != CircuitClosed {
		t.Fatal("healthy concurrent probes should close circuit")
	}
}

func TestListenerReasonsAndPanicsDoNotBreakStateMachine(t *testing.T) {
	clock := newManualClock()
	var mu sync.Mutex
	changes := make([]StateChangeReason, 0, 3)
	listener := func(change StateChange) {
		mu.Lock()
		changes = append(changes, change.Reason)
		mu.Unlock()
		if change.Reason == ReasonCooldownElapsed {
			panic("observer failure")
		}
	}
	breaker := newTestBreaker(t, clock, WithStateChangeListener(listener))
	openTestBreaker(t, breaker)
	clock.Advance(5 * time.Second)
	permits := make([]*CircuitPermit, 0, 3)
	for i := 0; i < 3; i++ {
		permit, err := breaker.TryAcquire()
		if err != nil {
			t.Fatal(err)
		}
		permits = append(permits, permit)
	}
	for _, permit := range permits {
		permit.RecordSuccess()
	}

	mu.Lock()
	defer mu.Unlock()
	want := []StateChangeReason{ReasonFailureRate, ReasonCooldownElapsed, ReasonHalfOpenSucceeded}
	if len(changes) != len(want) {
		t.Fatalf("unexpected changes: %v", changes)
	}
	for i := range want {
		if changes[i] != want[i] {
			t.Fatalf("unexpected changes: %v", changes)
		}
	}
}

func TestExecuteClassifierAndPanicHandling(t *testing.T) {
	clock := newManualClock()
	config := testCircuitConfig()
	config.MinimumRequests = 1
	breaker, err := NewCircuitBreaker(config, withCircuitBreakerClock(clock.Now))
	if err != nil {
		t.Fatal(err)
	}
	businessErr := errors.New("out of stock")
	_, err = Execute(breaker, func() (int, error) { return 0, businessErr }, func(error) bool { return false })
	if !errors.Is(err, businessErr) {
		t.Fatalf("expected business error, got %v", err)
	}
	if breaker.Snapshot().WindowRequests != 0 || breaker.State() != CircuitClosed {
		t.Fatal("business error should be ignored")
	}

	func() {
		defer func() {
			if recover() == nil {
				t.Fatal("expected panic")
			}
		}()
		_, _ = Execute(breaker, func() (int, error) { panic("boom") }, nil)
	}()
	if breaker.State() != CircuitOpen {
		t.Fatal("panic should be recorded as failure")
	}
}
