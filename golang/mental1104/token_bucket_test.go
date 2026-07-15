package mental1104

import (
	"context"
	"errors"
	"math"
	"sync"
	"testing"
	"time"
)

func TestNewTokenBucketRejectsInvalidConfiguration(t *testing.T) {
	for _, rate := range []float64{0, -1, math.NaN(), math.Inf(1), math.Inf(-1)} {
		if _, err := NewTokenBucket(rate, 1); err == nil {
			t.Fatalf("rate %v should be rejected", rate)
		}
	}
	for _, capacity := range []int{0, -1} {
		if _, err := NewTokenBucket(1, capacity); err == nil {
			t.Fatalf("capacity %d should be rejected", capacity)
		}
	}
}

func TestTokenBucketStartsFullAndReplenishesLazily(t *testing.T) {
	bucket, err := NewTokenBucket(20, 2)
	if err != nil {
		t.Fatal(err)
	}
	ctx := context.Background()
	if err := bucket.Acquire(ctx); err != nil {
		t.Fatal(err)
	}
	if err := bucket.Acquire(ctx); err != nil {
		t.Fatal(err)
	}

	started := time.Now()
	if err := bucket.Acquire(ctx); err != nil {
		t.Fatal(err)
	}
	elapsed := time.Since(started)
	if elapsed < 30*time.Millisecond || elapsed > 500*time.Millisecond {
		t.Fatalf("unexpected wait: %v", elapsed)
	}
}

func TestTokenBucketReleaseDoesNotReturnToken(t *testing.T) {
	bucket, _ := NewTokenBucket(0.1, 1)
	if err := bucket.Acquire(context.Background()); err != nil {
		t.Fatal(err)
	}
	bucket.Release()

	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if err := bucket.Acquire(ctx); !errors.Is(err, context.Canceled) {
		t.Fatalf("expected cancellation, got %v", err)
	}
}

func TestTokenBucketCancellationWakesWaiter(t *testing.T) {
	bucket, _ := NewTokenBucket(0.1, 1)
	if err := bucket.Acquire(context.Background()); err != nil {
		t.Fatal(err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	result := make(chan error, 1)
	go func() { result <- bucket.Acquire(ctx) }()
	time.Sleep(30 * time.Millisecond)
	cancel()

	select {
	case err := <-result:
		if !errors.Is(err, context.Canceled) {
			t.Fatalf("expected context cancellation, got %v", err)
		}
	case <-time.After(500 * time.Millisecond):
		t.Fatal("cancelled waiter did not wake")
	}
}

func TestTokenBucketConcurrentWaitersDoNotShareToken(t *testing.T) {
	bucket, _ := NewTokenBucket(0.01, 1)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	start := make(chan struct{})
	results := make(chan error, 4)
	var ready sync.WaitGroup
	ready.Add(4)
	for i := 0; i < 4; i++ {
		go func() {
			ready.Done()
			<-start
			results <- bucket.Acquire(ctx)
		}()
	}
	ready.Wait()
	close(start)

	first := <-results
	if first != nil {
		t.Fatalf("first waiter should acquire initial token: %v", first)
	}
	cancel()

	cancelled := 0
	for i := 0; i < 3; i++ {
		select {
		case err := <-results:
			if errors.Is(err, context.Canceled) {
				cancelled++
			}
		case <-time.After(500 * time.Millisecond):
			t.Fatal("waiter did not finish after cancellation")
		}
	}
	if cancelled != 3 {
		t.Fatalf("expected 3 cancelled waiters, got %d", cancelled)
	}
}

func TestTokenBucketRejectsNilContext(t *testing.T) {
	bucket, _ := NewTokenBucket(1, 1)
	if !errors.Is(bucket.Acquire(nil), ErrNilContext) {
		t.Fatal("nil context should return ErrNilContext")
	}
}
