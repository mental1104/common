package retry

import (
	"context"
	"errors"
	"testing"
	"time"
)

func BenchmarkDoSuccess(b *testing.B) {
	opts := Options{
		MaxAttempts: 3,
		BaseDelay:   time.Millisecond,
		MaxDelay:    time.Second,
	}

	b.ReportAllocs()
	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		if err := Do(context.Background(), opts, func() error { return nil }); err != nil {
			b.Fatal(err)
		}
	}
}

func BenchmarkDoRetryOnce(b *testing.B) {
	testErr := errors.New("temporary")
	opts := Options{
		MaxAttempts: 3,
		BaseDelay:   time.Millisecond,
		MaxDelay:    time.Second,
		JitterRatio: 0.2,
		Retryable:   func(error) bool { return true },
		Random:      func() float64 { return 0.5 },
		Sleep:       func(context.Context, time.Duration) error { return nil },
	}

	b.ReportAllocs()
	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		attempts := 0
		err := Do(context.Background(), opts, func() error {
			attempts++
			if attempts == 1 {
				return testErr
			}
			return nil
		})
		if err != nil {
			b.Fatal(err)
		}
	}
}
