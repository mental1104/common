package retry

import (
	"context"
	"errors"
	"math/rand"
	"slices"
	"testing"
	"time"
)

func TestBackoffSequence(t *testing.T) {
	want := []time.Duration{
		100 * time.Millisecond,
		200 * time.Millisecond,
		400 * time.Millisecond,
		500 * time.Millisecond,
		500 * time.Millisecond,
	}

	for attempt, expected := range want {
		got := Backoff(100*time.Millisecond, 500*time.Millisecond, attempt)
		if got != expected {
			t.Fatalf("attempt=%d: got=%v want=%v", attempt, got, expected)
		}
	}
}

func TestBackoffClampsBaseToMax(t *testing.T) {
	got := Backoff(time.Second, 100*time.Millisecond, 0)
	if got != 100*time.Millisecond {
		t.Fatalf("got=%v want=100ms", got)
	}
}

func TestJitterBoundary(t *testing.T) {
	tests := []struct {
		sample float64
		want   time.Duration
	}{
		{sample: 0, want: 80 * time.Millisecond},
		{sample: 0.5, want: 100 * time.Millisecond},
		{sample: 1, want: 120 * time.Millisecond},
	}

	for _, tt := range tests {
		got := Jitter(100*time.Millisecond, 0.2, tt.sample)
		if got != tt.want {
			t.Fatalf("sample=%v: got=%v want=%v", tt.sample, got, tt.want)
		}
	}
}

func TestJitterDistribution(t *testing.T) {
	rng := rand.New(rand.NewSource(1))
	const samples = 10000

	minDelay := time.Duration(1<<63 - 1)
	var maxDelay time.Duration
	var total time.Duration

	for i := 0; i < samples; i++ {
		got := Jitter(100*time.Millisecond, 0.2, rng.Float64())
		if got < 80*time.Millisecond || got > 120*time.Millisecond {
			t.Fatalf("out of range: %v", got)
		}
		if got < minDelay {
			minDelay = got
		}
		if got > maxDelay {
			maxDelay = got
		}
		total += got
	}

	mean := total / samples
	if mean < 99*time.Millisecond || mean > 101*time.Millisecond {
		t.Fatalf("mean=%v want approximately 100ms", mean)
	}
	t.Logf("samples=%d min=%v max=%v mean=%v", samples, minDelay, maxDelay, mean)
}

func TestNonRetryableStopsImmediately(t *testing.T) {
	attempts := 0
	sleepCalls := 0
	targetErr := errors.New("invalid request")

	err := Do(context.Background(), Options{
		MaxAttempts: 5,
		BaseDelay:   100 * time.Millisecond,
		MaxDelay:    time.Second,
		Retryable:   func(error) bool { return false },
		Random:      func() float64 { return 0.5 },
		Sleep: func(context.Context, time.Duration) error {
			sleepCalls++
			return nil
		},
	}, func() error {
		attempts++
		return targetErr
	})

	if !errors.Is(err, targetErr) {
		t.Fatalf("got=%v want=%v", err, targetErr)
	}
	if attempts != 1 {
		t.Fatalf("attempts=%d want=1", attempts)
	}
	if sleepCalls != 0 {
		t.Fatalf("sleepCalls=%d want=0", sleepCalls)
	}
}

func TestDeadlineStopsBeforeSleep(t *testing.T) {
	now := time.Now()
	ctx, cancel := context.WithDeadline(context.Background(), now.Add(time.Hour))
	defer cancel()

	attempts := 0
	sleepCalls := 0

	err := Do(ctx, Options{
		MaxAttempts: 5,
		BaseDelay:   2 * time.Hour,
		MaxDelay:    2 * time.Hour,
		Retryable:   func(error) bool { return true },
		Random:      func() float64 { return 0.5 },
		Now:         func() time.Time { return now },
		Sleep: func(context.Context, time.Duration) error {
			sleepCalls++
			return nil
		},
	}, func() error {
		attempts++
		return errors.New("temporary failure")
	})

	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("got=%v want=%v", err, context.DeadlineExceeded)
	}
	if attempts != 1 {
		t.Fatalf("attempts=%d want=1", attempts)
	}
	if sleepCalls != 0 {
		t.Fatalf("sleepCalls=%d want=0", sleepCalls)
	}
}

func TestRetrySequence(t *testing.T) {
	attempts := 0
	var sleeps []time.Duration
	targetErr := errors.New("temporary failure")

	err := Do(context.Background(), Options{
		MaxAttempts: 4,
		BaseDelay:   100 * time.Millisecond,
		MaxDelay:    time.Second,
		JitterRatio: 0.2,
		Retryable:   func(error) bool { return true },
		Random:      func() float64 { return 0.5 },
		Sleep: func(_ context.Context, delay time.Duration) error {
			sleeps = append(sleeps, delay)
			return nil
		},
	}, func() error {
		attempts++
		return targetErr
	})

	want := []time.Duration{
		100 * time.Millisecond,
		200 * time.Millisecond,
		400 * time.Millisecond,
	}
	if !errors.Is(err, targetErr) {
		t.Fatalf("got=%v want=%v", err, targetErr)
	}
	if attempts != 4 {
		t.Fatalf("attempts=%d want=4", attempts)
	}
	if !slices.Equal(sleeps, want) {
		t.Fatalf("sleeps=%v want=%v", sleeps, want)
	}
}

func TestSleepContextStopsWhenCanceled(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	err := sleepContext(ctx, time.Second)
	if !errors.Is(err, context.Canceled) {
		t.Fatalf("got=%v want=%v", err, context.Canceled)
	}
}

func TestTotalDeadlineTruncatesNextBackoff(t *testing.T) {
	start := time.Now()
	now := start
	ctx, cancel := context.WithDeadline(context.Background(), start.Add(250*time.Millisecond))
	defer cancel()

	attempts := 0
	sleepCalls := 0

	err := Do(ctx, Options{
		MaxAttempts: 5,
		BaseDelay:   100 * time.Millisecond,
		MaxDelay:    time.Second,
		Retryable:   func(error) bool { return true },
		Random:      func() float64 { return 0.5 },
		Now:         func() time.Time { return now },
		Sleep: func(_ context.Context, delay time.Duration) error {
			sleepCalls++
			now = now.Add(delay)
			return nil
		},
	}, func() error {
		attempts++
		return errors.New("temporary failure")
	})

	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("got=%v want=%v", err, context.DeadlineExceeded)
	}
	if attempts != 2 {
		t.Fatalf("attempts=%d want=2", attempts)
	}
	if sleepCalls != 1 {
		t.Fatalf("sleepCalls=%d want=1", sleepCalls)
	}
}

func TestInvalidOptions(t *testing.T) {
	err := Do(context.Background(), Options{JitterRatio: 1.1}, func() error { return nil })
	if !errors.Is(err, ErrInvalidOptions) {
		t.Fatalf("got=%v want=%v", err, ErrInvalidOptions)
	}
}

func TestNilInputs(t *testing.T) {
	if err := Do(nil, Options{}, func() error { return nil }); !errors.Is(err, ErrNilContext) {
		t.Fatalf("nil context: got=%v want=%v", err, ErrNilContext)
	}
	if err := Do(context.Background(), Options{}, nil); !errors.Is(err, ErrNilOperation) {
		t.Fatalf("nil operation: got=%v want=%v", err, ErrNilOperation)
	}
}
