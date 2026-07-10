package retry

import (
	"context"
	"errors"
	"fmt"
	"math/rand"
	"time"
)

var (
	// ErrInvalidOptions 表示重试参数不合法。
	ErrInvalidOptions = errors.New("retry: invalid options")
	// ErrNilContext 表示调用方传入了 nil context。
	ErrNilContext = errors.New("retry: nil context")
	// ErrNilOperation 表示调用方没有提供待执行函数。
	ErrNilOperation = errors.New("retry: nil operation")
)

// RetryableFunc 判断一次失败是否值得继续重试。
type RetryableFunc func(error) bool

// Options 控制重试次数、退避时间、抖动和错误分类。
//
// MaxAttempts 包含第一次调用；小于等于 0 时按 1 次处理。
// MaxDelay 为 0 时使用 BaseDelay。JitterRatio 必须位于 [0, 1]。
// Sleep、Now 和 Random 主要用于测试注入，生产调用通常留空。
type Options struct {
	MaxAttempts int
	BaseDelay   time.Duration
	MaxDelay    time.Duration
	JitterRatio float64
	Retryable   RetryableFunc
	Sleep       func(context.Context, time.Duration) error
	Now         func() time.Time
	Random      func() float64
}

// Backoff 计算第 attempt 次等待的指数退避时间。
// attempt 从 0 开始，结果不会超过 max；max 小于等于 0 时按 base 处理。
func Backoff(base, max time.Duration, attempt int) time.Duration {
	if base <= 0 {
		return 0
	}
	if max <= 0 {
		max = base
	}
	if base > max {
		base = max
	}
	if attempt <= 0 {
		return base
	}

	delay := base
	for i := 0; i < attempt; i++ {
		if delay >= max || delay > max/2 {
			return max
		}
		delay *= 2
	}
	return delay
}

// Jitter 在 delay 上施加对称抖动。
// ratio=0.2 时结果位于原始 delay 的 80% 到 120% 之间。
// sample 会被限制在 [0, 1]，ratio 大于 1 时按 1 处理。
func Jitter(delay time.Duration, ratio, sample float64) time.Duration {
	if delay <= 0 || ratio <= 0 {
		return delay
	}
	if ratio > 1 {
		ratio = 1
	}
	if sample < 0 {
		sample = 0
	} else if sample > 1 {
		sample = 1
	}

	factor := 1 - ratio + 2*ratio*sample
	return time.Duration(float64(delay) * factor)
}

// Do 执行 fn，失败时依据 opts 进行重试。
//
// 每次等待前都会检查 context deadline。若剩余时间不足以容纳下一次退避，
// Do 会直接返回 context.DeadlineExceeded，不会继续调用 Sleep。
func Do(ctx context.Context, opts Options, fn func() error) error {
	if ctx == nil {
		return ErrNilContext
	}
	if fn == nil {
		return ErrNilOperation
	}
	if err := normalize(&opts); err != nil {
		return err
	}

	var lastErr error
	for attempt := 0; attempt < opts.MaxAttempts; attempt++ {
		if err := ctx.Err(); err != nil {
			return err
		}

		lastErr = fn()
		if lastErr == nil {
			return nil
		}
		if !opts.Retryable(lastErr) || attempt == opts.MaxAttempts-1 {
			return lastErr
		}

		delay := Backoff(opts.BaseDelay, opts.MaxDelay, attempt)
		delay = Jitter(delay, opts.JitterRatio, opts.Random())

		if deadline, ok := ctx.Deadline(); ok && !opts.Now().Add(delay).Before(deadline) {
			return context.DeadlineExceeded
		}
		if err := opts.Sleep(ctx, delay); err != nil {
			return err
		}
	}

	return lastErr
}

func normalize(opts *Options) error {
	if opts.MaxAttempts <= 0 {
		opts.MaxAttempts = 1
	}
	if opts.BaseDelay < 0 {
		return fmt.Errorf("%w: BaseDelay must not be negative", ErrInvalidOptions)
	}
	if opts.MaxDelay < 0 {
		return fmt.Errorf("%w: MaxDelay must not be negative", ErrInvalidOptions)
	}
	if opts.JitterRatio < 0 || opts.JitterRatio > 1 {
		return fmt.Errorf("%w: JitterRatio must be within [0, 1]", ErrInvalidOptions)
	}
	if opts.MaxDelay == 0 {
		opts.MaxDelay = opts.BaseDelay
	}
	if opts.Retryable == nil {
		opts.Retryable = func(error) bool { return true }
	}
	if opts.Sleep == nil {
		opts.Sleep = sleepContext
	}
	if opts.Now == nil {
		opts.Now = time.Now
	}
	if opts.Random == nil {
		opts.Random = rand.Float64
	}
	return nil
}

func sleepContext(ctx context.Context, delay time.Duration) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if delay <= 0 {
		return nil
	}

	timer := time.NewTimer(delay)
	defer timer.Stop()

	select {
	case <-timer.C:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}
