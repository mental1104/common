package mental1104

import (
	"errors"
	"fmt"
	"sync"
	"sync/atomic"
	"time"
)

// CircuitState is the current state of a CircuitBreaker.
type CircuitState string

const (
	CircuitClosed   CircuitState = "closed"
	CircuitOpen     CircuitState = "open"
	CircuitHalfOpen CircuitState = "half_open"
)

// StateChangeReason explains why a circuit breaker changed state.
type StateChangeReason string

const (
	ReasonFailureRate       StateChangeReason = "failure_rate"
	ReasonSlowCallRate      StateChangeReason = "slow_call_rate"
	ReasonCooldownElapsed   StateChangeReason = "cooldown_elapsed"
	ReasonHalfOpenSucceeded StateChangeReason = "half_open_succeeded"
	ReasonHalfOpenFailed    StateChangeReason = "half_open_failed"
)

// ErrCircuitOpen is matched by errors.Is when a call is rejected locally.
var ErrCircuitOpen = errors.New("circuit breaker: open")

// CircuitOpenError describes a local fast-fail rejection.
type CircuitOpenError struct {
	RetryAfter time.Duration
}

func (e *CircuitOpenError) Error() string {
	return fmt.Sprintf("circuit breaker is open; retry after %s", e.RetryAfter)
}

func (e *CircuitOpenError) Unwrap() error { return ErrCircuitOpen }

// CircuitBreakerConfig controls the sliding window and state transitions.
type CircuitBreakerConfig struct {
	Window                   time.Duration
	MinimumRequests          int
	FailureRateThreshold     float64
	SlowCallDuration         time.Duration
	SlowCallRateThreshold    float64
	OpenDuration             time.Duration
	HalfOpenMaxProbes        int
	HalfOpenSuccessesToClose int
}

// DefaultCircuitBreakerConfig returns the example production-oriented defaults.
func DefaultCircuitBreakerConfig() CircuitBreakerConfig {
	return CircuitBreakerConfig{
		Window:                   10 * time.Second,
		MinimumRequests:          20,
		FailureRateThreshold:     0.5,
		SlowCallDuration:         800 * time.Millisecond,
		SlowCallRateThreshold:    0.6,
		OpenDuration:             5 * time.Second,
		HalfOpenMaxProbes:        3,
		HalfOpenSuccessesToClose: 3,
	}
}

func (c CircuitBreakerConfig) validate() error {
	if c.Window <= 0 {
		return errors.New("circuit breaker: window must be positive")
	}
	if c.MinimumRequests <= 0 {
		return errors.New("circuit breaker: minimum requests must be positive")
	}
	if c.FailureRateThreshold < 0 || c.FailureRateThreshold > 1 {
		return errors.New("circuit breaker: failure rate threshold must be in [0, 1]")
	}
	if c.SlowCallDuration <= 0 {
		return errors.New("circuit breaker: slow call duration must be positive")
	}
	if c.SlowCallRateThreshold < 0 || c.SlowCallRateThreshold > 1 {
		return errors.New("circuit breaker: slow call rate threshold must be in [0, 1]")
	}
	if c.OpenDuration <= 0 {
		return errors.New("circuit breaker: open duration must be positive")
	}
	if c.HalfOpenMaxProbes <= 0 {
		return errors.New("circuit breaker: half-open max probes must be positive")
	}
	if c.HalfOpenSuccessesToClose <= 0 {
		return errors.New("circuit breaker: half-open successes to close must be positive")
	}
	if c.HalfOpenSuccessesToClose > c.HalfOpenMaxProbes {
		return errors.New("circuit breaker: half-open successes to close must not exceed max probes")
	}
	return nil
}

// StateChange is delivered after a state transition, outside the breaker lock.
type StateChange struct {
	PreviousState CircuitState
	NewState      CircuitState
	Reason        StateChangeReason
	At            time.Time
	Generation    uint64
}

// StateChangeListener receives state changes. Panics are recovered by the breaker.
type StateChangeListener func(StateChange)

// CircuitBreakerSnapshot is a thread-safe point-in-time view of breaker state.
type CircuitBreakerSnapshot struct {
	State             CircuitState
	Generation        uint64
	WindowRequests    int
	WindowFailures    int
	WindowSlowCalls   int
	FailureRate       float64
	SlowCallRate      float64
	HalfOpenIssued    int
	HalfOpenInFlight  int
	HalfOpenSuccesses int
	RetryAfter        time.Duration
}

// CircuitBreakerOption customizes CircuitBreaker construction.
type CircuitBreakerOption func(*CircuitBreaker)

// WithStateChangeListener installs an observability hook.
func WithStateChangeListener(listener StateChangeListener) CircuitBreakerOption {
	return func(b *CircuitBreaker) { b.listener = listener }
}

type windowEvent struct {
	at      time.Time
	failure bool
	slow    bool
}

type circuitOutcome uint8

const (
	outcomeSuccess circuitOutcome = iota
	outcomeFailure
	outcomeIgnored
)

// CircuitBreaker is a process-local, thread-safe circuit breaker.
type CircuitBreaker struct {
	mu       sync.Mutex
	config   CircuitBreakerConfig
	now      func() time.Time
	listener StateChangeListener

	state      CircuitState
	generation uint64
	openedAt   time.Time
	events     []windowEvent

	halfOpenIssued    int
	halfOpenInFlight  int
	halfOpenSuccesses int
}

// NewCircuitBreaker validates config and returns a closed breaker.
func NewCircuitBreaker(config CircuitBreakerConfig, options ...CircuitBreakerOption) (*CircuitBreaker, error) {
	if err := config.validate(); err != nil {
		return nil, err
	}
	breaker := &CircuitBreaker{
		config: config,
		now:    time.Now,
		state:  CircuitClosed,
		events: make([]windowEvent, 0, config.MinimumRequests),
	}
	for _, option := range options {
		if option != nil {
			option(breaker)
		}
	}
	return breaker, nil
}

func withCircuitBreakerClock(now func() time.Time) CircuitBreakerOption {
	return func(b *CircuitBreaker) { b.now = now }
}

// CircuitPermit is a one-shot permission to call downstream.
type CircuitPermit struct {
	breaker    *CircuitBreaker
	generation uint64
	state      CircuitState
	startedAt  time.Time
	completed  atomic.Bool
}

// State returns the state in which this permit was issued.
func (p *CircuitPermit) State() CircuitState { return p.state }

// RecordSuccess completes the permit as a successful call.
func (p *CircuitPermit) RecordSuccess() bool { return p.complete(outcomeSuccess) }

// RecordFailure completes the permit as a system failure.
func (p *CircuitPermit) RecordFailure() bool { return p.complete(outcomeFailure) }

// RecordIgnored completes the permit as a business outcome excluded from the closed window.
func (p *CircuitPermit) RecordIgnored() bool { return p.complete(outcomeIgnored) }

func (p *CircuitPermit) complete(outcome circuitOutcome) bool {
	if !p.completed.CompareAndSwap(false, true) {
		return false
	}
	p.breaker.complete(p.generation, p.state, p.startedAt, outcome)
	return true
}

// TryAcquire returns a permit or a CircuitOpenError without calling downstream.
func (b *CircuitBreaker) TryAcquire() (*CircuitPermit, error) {
	var change *StateChange
	b.mu.Lock()
	now := b.now()

	if b.state == CircuitOpen {
		retryAfter := b.retryAfterLocked(now)
		if retryAfter > 0 {
			b.mu.Unlock()
			return nil, &CircuitOpenError{RetryAfter: retryAfter}
		}
		change = b.transitionLocked(CircuitHalfOpen, ReasonCooldownElapsed, now)
	}

	if b.state == CircuitHalfOpen {
		if b.halfOpenIssued >= b.config.HalfOpenMaxProbes {
			b.mu.Unlock()
			return nil, &CircuitOpenError{}
		}
		b.halfOpenIssued++
		b.halfOpenInFlight++
	}

	permit := &CircuitPermit{
		breaker:    b,
		generation: b.generation,
		state:      b.state,
		startedAt:  now,
	}
	b.mu.Unlock()
	b.notify(change)
	return permit, nil
}

// State returns the current state without forcing Open to HalfOpen.
func (b *CircuitBreaker) State() CircuitState {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.state
}

// Snapshot returns current window counters and HalfOpen progress.
func (b *CircuitBreaker) Snapshot() CircuitBreakerSnapshot {
	b.mu.Lock()
	defer b.mu.Unlock()

	now := b.now()
	if b.state == CircuitClosed {
		b.pruneLocked(now)
	}
	requests, failures, slowCalls := b.countsLocked()
	failureRate, slowRate := 0.0, 0.0
	if requests > 0 {
		failureRate = float64(failures) / float64(requests)
		slowRate = float64(slowCalls) / float64(requests)
	}
	return CircuitBreakerSnapshot{
		State:             b.state,
		Generation:        b.generation,
		WindowRequests:    requests,
		WindowFailures:    failures,
		WindowSlowCalls:   slowCalls,
		FailureRate:       failureRate,
		SlowCallRate:      slowRate,
		HalfOpenIssued:    b.halfOpenIssued,
		HalfOpenInFlight:  b.halfOpenInFlight,
		HalfOpenSuccesses: b.halfOpenSuccesses,
		RetryAfter:        b.retryAfterLocked(now),
	}
}

func (b *CircuitBreaker) complete(generation uint64, acquiredState CircuitState, startedAt time.Time, outcome circuitOutcome) {
	var change *StateChange
	b.mu.Lock()
	now := b.now()
	if generation != b.generation || acquiredState != b.state {
		b.mu.Unlock()
		return
	}

	duration := now.Sub(startedAt)
	if duration < 0 {
		duration = 0
	}
	slow := duration >= b.config.SlowCallDuration

	switch b.state {
	case CircuitClosed:
		if outcome != outcomeIgnored {
			b.events = append(b.events, windowEvent{
				at:      now,
				failure: outcome == outcomeFailure,
				slow:    slow,
			})
			b.pruneLocked(now)
			change = b.evaluateClosedLocked(now)
		}
	case CircuitHalfOpen:
		b.halfOpenInFlight--
		if outcome == outcomeFailure || slow {
			change = b.transitionLocked(CircuitOpen, ReasonHalfOpenFailed, now)
		} else {
			b.halfOpenSuccesses++
			if b.halfOpenSuccesses >= b.config.HalfOpenSuccessesToClose && b.halfOpenInFlight == 0 {
				change = b.transitionLocked(CircuitClosed, ReasonHalfOpenSucceeded, now)
			}
		}
	}
	b.mu.Unlock()
	b.notify(change)
}

func (b *CircuitBreaker) evaluateClosedLocked(now time.Time) *StateChange {
	requests, failures, slowCalls := b.countsLocked()
	if requests < b.config.MinimumRequests {
		return nil
	}
	if float64(failures)/float64(requests) >= b.config.FailureRateThreshold {
		return b.transitionLocked(CircuitOpen, ReasonFailureRate, now)
	}
	if float64(slowCalls)/float64(requests) >= b.config.SlowCallRateThreshold {
		return b.transitionLocked(CircuitOpen, ReasonSlowCallRate, now)
	}
	return nil
}

func (b *CircuitBreaker) countsLocked() (requests, failures, slowCalls int) {
	requests = len(b.events)
	for _, event := range b.events {
		if event.failure {
			failures++
		}
		if event.slow {
			slowCalls++
		}
	}
	return requests, failures, slowCalls
}

func (b *CircuitBreaker) pruneLocked(now time.Time) {
	cutoff := now.Add(-b.config.Window)
	firstValid := 0
	for firstValid < len(b.events) && b.events[firstValid].at.Before(cutoff) {
		firstValid++
	}
	if firstValid == 0 {
		return
	}
	copy(b.events, b.events[firstValid:])
	b.events = b.events[:len(b.events)-firstValid]
}

func (b *CircuitBreaker) retryAfterLocked(now time.Time) time.Duration {
	if b.state != CircuitOpen {
		return 0
	}
	retryAfter := b.openedAt.Add(b.config.OpenDuration).Sub(now)
	if retryAfter < 0 {
		return 0
	}
	return retryAfter
}

func (b *CircuitBreaker) transitionLocked(newState CircuitState, reason StateChangeReason, now time.Time) *StateChange {
	change := &StateChange{
		PreviousState: b.state,
		NewState:      newState,
		Reason:        reason,
		At:            now,
		Generation:    b.generation + 1,
	}
	b.state = newState
	b.generation++
	b.events = b.events[:0]
	b.halfOpenIssued = 0
	b.halfOpenInFlight = 0
	b.halfOpenSuccesses = 0
	if newState == CircuitOpen {
		b.openedAt = now
	} else {
		b.openedAt = time.Time{}
	}
	return change
}

func (b *CircuitBreaker) notify(change *StateChange) {
	if change == nil || b.listener == nil {
		return
	}
	func() {
		defer func() { _ = recover() }()
		b.listener(*change)
	}()
}

// ErrorClassifier returns true for system failures and false for ignored business errors.
type ErrorClassifier func(error) bool

// Execute runs operation when permitted and records its result.
func Execute[T any](breaker *CircuitBreaker, operation func() (T, error), classifier ErrorClassifier) (T, error) {
	return execute(breaker, operation, classifier, nil)
}

// ExecuteOrFallback runs fallback only when the breaker rejects the call locally.
func ExecuteOrFallback[T any](
	breaker *CircuitBreaker,
	operation func() (T, error),
	classifier ErrorClassifier,
	fallback func(*CircuitOpenError) (T, error),
) (T, error) {
	return execute(breaker, operation, classifier, fallback)
}

func execute[T any](
	breaker *CircuitBreaker,
	operation func() (T, error),
	classifier ErrorClassifier,
	fallback func(*CircuitOpenError) (T, error),
) (result T, err error) {
	permit, acquireErr := breaker.TryAcquire()
	if acquireErr != nil {
		var openErr *CircuitOpenError
		if fallback != nil && errors.As(acquireErr, &openErr) {
			return fallback(openErr)
		}
		return result, acquireErr
	}

	defer func() {
		if recovered := recover(); recovered != nil {
			permit.RecordFailure()
			panic(recovered)
		}
	}()

	result, err = operation()
	if err != nil {
		if classifier == nil || classifier(err) {
			permit.RecordFailure()
		} else {
			permit.RecordIgnored()
		}
		return result, err
	}
	permit.RecordSuccess()
	return result, nil
}
