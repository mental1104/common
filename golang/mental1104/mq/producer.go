package mq

import (
	"context"
	"errors"
	"sync"
	"sync/atomic"
)

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

func closedSignal() chan struct{} {
	ch := make(chan struct{})
	close(ch)
	return ch
}

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

func (s *producerState) finishSync() {
	s.mu.Lock()
	s.syncActive--
	if s.syncActive == 0 {
		close(s.syncIdle)
	}
	s.mu.Unlock()
}

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

func waitFor(ctx context.Context, ch <-chan struct{}) error {
	select {
	case <-ch:
		return nil
	case <-ctx.Done():
		return NormalizeError(ctx.Err(), ErrorCanceled, "wait for message queue operation", "")
	}
}

func (s *producerState) close(ctx context.Context) error {
	s.mu.Lock()
	if s.closing {
		done := s.closeDone
		s.mu.Unlock()
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

type Producer struct {
	state *producerState
}

func NewProducer(backend ProducerBackend) (*Producer, error) {
	state, err := newProducerState(backend)
	if err != nil {
		return nil, err
	}
	return &Producer{state: state}, nil
}

func (p *Producer) Async() *AsyncProducer {
	if p == nil {
		return &AsyncProducer{}
	}
	return &AsyncProducer{state: p.state}
}

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

func (p *Producer) Close(ctx context.Context) error {
	if p == nil || p.state == nil {
		return nil
	}
	return p.state.close(ctx)
}

type AsyncProducer struct {
	state *producerState
}

func (p *AsyncProducer) SendAsync(ctx context.Context, message Message, callback DeliveryCallback) error {
	if p == nil || p.state == nil {
		return NewError(ErrorInvalidConfig, "send async message", "", "async producer is nil", nil)
	}
	if err := p.state.beginAsync(); err != nil {
		return err
	}

	var once sync.Once
	var callbackStarted atomic.Bool
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
		if callbackStarted.Load() {
			return nil
		}
		complete(SendResult{}, false)
		return NormalizeError(err, ErrorBackend, "submit async message", "")
	}
	return nil
}

func (p *AsyncProducer) Close(ctx context.Context) error {
	if p == nil || p.state == nil {
		return nil
	}
	return p.state.close(ctx)
}
