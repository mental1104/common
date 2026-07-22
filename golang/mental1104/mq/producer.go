package mq

import (
	"context"
	"errors"
	"sync"
	"sync/atomic"
)

// producerState 保存同步 Producer 与 AsyncProducer 共享的后端和生命周期状态。
// mu 保护关闭标志、操作计数和等待 channel；backend 本身负责 SDK 级并发安全。
type producerState struct {
	backend ProducerBackend

	mu           sync.Mutex
	closed       bool
	closing      bool
	closeDone    chan struct{}
	closeErr     error
	syncActive   int
	asyncPending int
	syncIdle     chan struct{}
	asyncIdle    chan struct{}
}

// closedSignal 创建一个已经关闭的 channel，用于表示当前没有在途操作。
func closedSignal() chan struct{} {
	ch := make(chan struct{})
	close(ch)
	return ch
}

// newProducerState 校验 backend 并初始化空闲信号。
// backend 由调用方持有的所有权转移到返回的共享状态中。
func newProducerState(backend ProducerBackend) (*producerState, error) {
	if backend == nil {
		return nil, NewError(ErrorInvalidConfig, "new producer", "", "producer backend must not be nil", nil)
	}
	return &producerState{
		backend:   backend,
		syncIdle:  closedSignal(),
		asyncIdle: closedSignal(),
	}, nil
}

// beginSync 在资源仍开放时登记一个同步发送。
// 第一个在途操作会替换空闲信号，供 Close 等待全部同步发送结束。
func (s *producerState) beginSync() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.closed {
		return ErrClosed
	}
	if s.syncActive == 0 {
		s.syncIdle = make(chan struct{})
	}
	s.syncActive++
	return nil
}

// finishSync 完成一个同步发送，并在计数归零时唤醒关闭流程。
func (s *producerState) finishSync() {
	s.mu.Lock()
	s.syncActive--
	if s.syncActive == 0 {
		close(s.syncIdle)
	}
	s.mu.Unlock()
}

// beginAsync 在资源仍开放时登记一个已提交的异步发送。
func (s *producerState) beginAsync() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.closed {
		return ErrClosed
	}
	if s.asyncPending == 0 {
		s.asyncIdle = make(chan struct{})
	}
	s.asyncPending++
	return nil
}

// finishAsync 完成一个异步请求。
// 对重复 backend callback 做防御性保护，避免计数降为负数或重复关闭 channel。
func (s *producerState) finishAsync() {
	s.mu.Lock()
	if s.asyncPending > 0 {
		s.asyncPending--
		if s.asyncPending == 0 {
			close(s.asyncIdle)
		}
	}
	s.mu.Unlock()
}

// waitFor 等待 channel 关闭或 ctx 取消。
// ctx 决定调用方愿意等待多久；取消不会反向强制终止底层 SDK 操作。
func waitFor(ctx context.Context, ch <-chan struct{}) error {
	select {
	case <-ch:
		return nil
	case <-ctx.Done():
		return NormalizeError(ctx.Err(), ErrorCanceled, "wait for message queue operation", "")
	}
}

// close 执行 Producer 和 AsyncProducer 共享的幂等关闭流程。
//
// 关闭顺序固定为：拒绝新请求、等待同步发送、关闭 backend、等待所有已接受
// 的异步请求完成。并发 Close 复用同一个 closeDone 和最终错误。
func (s *producerState) close(ctx context.Context) error {
	s.mu.Lock()
	if s.closing {
		done := s.closeDone
		s.mu.Unlock()
		// 另一个调用方已经负责真正关闭；当前调用只等待同一结果。
		if err := waitFor(ctx, done); err != nil {
			return err
		}
		s.mu.Lock()
		err := s.closeErr
		s.mu.Unlock()
		return err
	}
	s.closing = true
	s.closed = true
	s.closeDone = make(chan struct{})
	syncIdle := s.syncIdle
	s.mu.Unlock()

	var closeErr error
	if err := waitFor(ctx, syncIdle); err != nil {
		closeErr = err
	} else {
		closeErr = NormalizeError(s.backend.Close(ctx), ErrorBackend, "close producer backend", "")
	}

	s.mu.Lock()
	asyncIdle := s.asyncIdle
	s.mu.Unlock()
	if err := waitFor(ctx, asyncIdle); err != nil {
		closeErr = errors.Join(closeErr, err)
	}

	s.mu.Lock()
	s.closeErr = closeErr
	close(s.closeDone)
	s.mu.Unlock()
	return closeErr
}

// Producer 是同步发送抽象，并可通过 Async 获取共享连接的异步 facade。
// Producer 可并发调用 Send；Close 幂等，关闭后不再接受新发送。
type Producer struct {
	state *producerState
}

// NewProducer 使用 backend 创建 Producer。
// backend 不能为 nil，其生命周期由 Producer/AsyncProducer 共享状态管理。
func NewProducer(backend ProducerBackend) (*Producer, error) {
	state, err := newProducerState(backend)
	if err != nil {
		return nil, err
	}
	return &Producer{state: state}, nil
}

// Async 返回共享同一 backend、连接和 Close 状态的 AsyncProducer。
// 返回对象不创建第二套 SDK client，也不转移 Producer 所有权。
func (p *Producer) Async() *AsyncProducer {
	if p == nil {
		return &AsyncProducer{}
	}
	return &AsyncProducer{state: p.state}
}

// Send 同步发送 message，并返回后端确认结果。
// ctx 控制发送取消和超时；Bridge 会复制消息可变字段并统一包装后端错误。
func (p *Producer) Send(ctx context.Context, message Message) (SendResult, error) {
	if p == nil || p.state == nil {
		err := NewError(ErrorInvalidConfig, "send message", "", "producer is nil", nil)
		return SendResult{Err: err}, err
	}
	if err := p.state.beginSync(); err != nil {
		return SendResult{Err: err}, err
	}
	defer p.state.finishSync()

	result, err := p.state.backend.Send(ctx, CloneMessage(message))
	if err != nil {
		err = NormalizeError(err, ErrorBackend, "send message", "")
		result.Err = err
		return result, err
	}
	if result.Err != nil {
		result.Err = NormalizeError(result.Err, ErrorBackend, "send message", "")
		return result, result.Err
	}
	return result, nil
}

// Close 幂等关闭共享 backend，并等待已接受的发送按既定语义收敛。
// nil Producer 视为已经关闭；ctx 只限制当前调用的等待时间。
func (p *Producer) Close(ctx context.Context) error {
	if p == nil || p.state == nil {
		return nil
	}
	return p.state.close(ctx)
}

// AsyncProducer 是 Producer 的异步 facade。
// 它不单独拥有连接，关闭任一 facade 都会关闭共享 Producer 状态。
type AsyncProducer struct {
	state *producerState
}

// SendAsync 提交异步发送。
//
// backend 同步拒绝时返回 error 且不调用 callback；一旦接受，callback 最终最多
// 调用一次。callback 在 backend 派发的 goroutine 中执行，panic 会被隔离。完成
// 计数在进入 callback 前释放，因此 callback 可以关闭同一个 Producer。
func (p *AsyncProducer) SendAsync(ctx context.Context, message Message, callback DeliveryCallback) error {
	if p == nil || p.state == nil {
		return NewError(ErrorInvalidConfig, "send async message", "", "async producer is nil", nil)
	}
	if err := p.state.beginAsync(); err != nil {
		return err
	}

	var once sync.Once
	var callbackStarted atomic.Bool
	// complete 同时承担 exactly-once 门禁和 pending 计数释放。
	complete := func(result SendResult, deliver bool) {
		once.Do(func() {
			callbackStarted.Store(true)
			p.state.finishAsync()
			if deliver && callback != nil {
				func() {
					defer func() { _ = recover() }()
					callback(result)
				}()
			}
		})
	}

	err := p.state.backend.SendAsync(ctx, CloneMessage(message), func(result SendResult) {
		if result.Err != nil {
			result.Err = NormalizeError(result.Err, ErrorBackend, "deliver async message", "")
		}
		complete(result, true)
	})
	if err != nil {
		// 极少数 SDK 可能在调用 callback 后仍返回错误；此时以已交付结果为准。
		if callbackStarted.Load() {
			return nil
		}
		complete(SendResult{}, false)
		return NormalizeError(err, ErrorBackend, "submit async message", "")
	}
	return nil
}

// Close 等同于共享 Producer 的 Close。
func (p *AsyncProducer) Close(ctx context.Context) error {
	if p == nil || p.state == nil {
		return nil
	}
	return p.state.close(ctx)
}
