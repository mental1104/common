package mental1104

import (
	"context"
	"errors"
	"math"
	"sync"
	"time"
)

var ErrNilContext = errors.New("token bucket: context must not be nil")

type TokenBucket struct {
	mu       sync.Mutex
	rate     float64
	capacity float64
	tokens   float64
	last     time.Time
}

func NewTokenBucket(rate float64, capacity int) (*TokenBucket, error) {
	if rate <= 0 || math.IsNaN(rate) || math.IsInf(rate, 0) {
		return nil, errors.New("token bucket: rate must be a finite positive number")
	}
	if capacity <= 0 {
		return nil, errors.New("token bucket: capacity must be positive")
	}

	return &TokenBucket{
		rate:     rate,
		capacity: float64(capacity),
		tokens:   float64(capacity),
		last:     time.Now(),
	}, nil
}

func (b *TokenBucket) Acquire(ctx context.Context) error {
	if ctx == nil {
		return ErrNilContext
	}

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}

		b.mu.Lock()
		now := time.Now()
		b.tokens = math.Min(
			b.capacity,
			b.tokens+now.Sub(b.last).Seconds()*b.rate,
		)
		b.last = now

		if b.tokens >= 1 {
			b.tokens--
			b.mu.Unlock()
			return nil
		}

		wait := durationUntilNextToken((1 - b.tokens) / b.rate)
		b.mu.Unlock()

		timer := time.NewTimer(wait)
		select {
		case <-timer.C:
		case <-ctx.Done():
			if !timer.Stop() {
				select {
				case <-timer.C:
				default:
				}
			}
			return ctx.Err()
		}
	}
}

func (b *TokenBucket) Release() {}

func durationUntilNextToken(seconds float64) time.Duration {
	const nanosPerSecond = float64(time.Second)
	maxSeconds := float64(math.MaxInt64) / nanosPerSecond
	if seconds >= maxSeconds {
		return time.Duration(math.MaxInt64)
	}

	nanoseconds := math.Ceil(seconds * nanosPerSecond)
	if nanoseconds < 1 {
		nanoseconds = 1
	}
	return time.Duration(nanoseconds)
}
