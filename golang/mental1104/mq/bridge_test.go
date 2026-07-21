package mq

import (
	"context"
	"errors"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

type fakeProducerBackend struct {
	sendResult SendResult
	sendErr    error
	asyncErr   error
	duplicate  bool
	closeErr   error
	closeCount atomic.Int32
}

func (f *fakeProducerBackend) Send(context.Context, Message) (SendResult, error) {
	return f.sendResult, f.sendErr
}
func (f *fakeProducerBackend) SendAsync(_ context.Context, _ Message, callback DeliveryCallback) error {
	if f.asyncErr != nil {
		return f.asyncErr
	}
	go func() {
		callback(f.sendResult)
		if f.duplicate {
			callback(f.sendResult)
		}
	}()
	return nil
}
func (f *fakeProducerBackend) Close(context.Context) error {
	f.closeCount.Add(1)
	return f.closeErr
}

type fakeConsumerBackend struct {
	mu         sync.Mutex
	messages   []BackendMessage
	receive    chan struct{}
	ackCount   atomic.Int32
	nackCount  atomic.Int32
	closeCount atomic.Int32
}

func newFakeConsumerBackend(messages ...BackendMessage) *fakeConsumerBackend {
	return &fakeConsumerBackend{messages: messages, receive: make(chan struct{}, 1)}
}
func (f *fakeConsumerBackend) Receive(ctx context.Context) (BackendMessage, error) {
	for {
		f.mu.Lock()
		if len(f.messages) > 0 {
			message := f.messages[0]
			f.messages = f.messages[1:]
			f.mu.Unlock()
			return message, nil
		}
		f.mu.Unlock()
		select {
		case <-ctx.Done():
			return BackendMessage{}, ctx.Err()
		case <-f.receive:
		}
	}
}
func (f *fakeConsumerBackend) Acknowledge(context.Context, string) error {
	f.ackCount.Add(1)
	return nil
}
func (f *fakeConsumerBackend) NegativeAcknowledge(context.Context, string) error {
	f.nackCount.Add(1)
	return nil
}
func (f *fakeConsumerBackend) Unsubscribe(context.Context) error { return nil }
func (f *fakeConsumerBackend) Resubscribe(context.Context) error { return nil }
func (f *fakeConsumerBackend) Close(context.Context) error {
	f.closeCount.Add(1)
	return nil
}

func TestProducerBridgeForwardsAndClosesIdempotently(t *testing.T) {
	backend := &fakeProducerBackend{sendResult: SendResult{MessageID: "42"}}
	producer, err := NewProducer(backend)
	if err != nil {
		t.Fatal(err)
	}
	result, err := producer.Send(context.Background(), NewMessage([]byte("hello")))
	if err != nil || result.MessageID != "42" {
		t.Fatalf("result=%+v err=%v", result, err)
	}
	if err := producer.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
	if err := producer.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
	if backend.closeCount.Load() != 1 {
		t.Fatalf("close count=%d", backend.closeCount.Load())
	}
	if _, err := producer.Send(context.Background(), Message{}); !errors.Is(err, ErrClosed) {
		t.Fatalf("expected ErrClosed, got %v", err)
	}
}

func TestProducerBridgeConvertsBackendError(t *testing.T) {
	backendErr := errors.New("broker unavailable")
	producer, _ := NewProducer(&fakeProducerBackend{sendErr: backendErr})
	_, err := producer.Send(context.Background(), Message{})
	var mqErr *MQError
	if !errors.As(err, &mqErr) || !errors.Is(err, backendErr) || mqErr.Code != ErrorBackend {
		t.Fatalf("unexpected error: %#v", err)
	}
}

func TestAsyncProducerCallbackExactlyOnceAndPanicSafe(t *testing.T) {
	backend := &fakeProducerBackend{sendResult: SendResult{MessageID: "async"}, duplicate: true}
	producer, _ := NewProducer(backend)
	async := producer.Async()
	var calls atomic.Int32
	done := make(chan struct{})
	if err := async.SendAsync(context.Background(), Message{}, func(result SendResult) {
		if calls.Add(1) == 1 {
			close(done)
		}
		panic("caller panic")
	}); err != nil {
		t.Fatal(err)
	}
	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("callback was not invoked")
	}
	if err := async.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
	if calls.Load() != 1 {
		t.Fatalf("callback count=%d", calls.Load())
	}
}

func TestAsyncProducerSynchronousRejectionDoesNotCallback(t *testing.T) {
	backendErr := errors.New("queue full")
	producer, _ := NewProducer(&fakeProducerBackend{asyncErr: backendErr})
	var calls atomic.Int32
	err := producer.Async().SendAsync(context.Background(), Message{}, func(SendResult) { calls.Add(1) })
	if err == nil || !errors.Is(err, backendErr) {
		t.Fatalf("expected wrapped rejection, got %v", err)
	}
	if calls.Load() != 0 {
		t.Fatalf("callback count=%d", calls.Load())
	}
}

func TestConsumerStartStopRestartAndHandlerFailure(t *testing.T) {
	backend := newFakeConsumerBackend(
		NewBackendMessage(NewMessage([]byte("one")), "r1"),
	)
	consumer, _ := NewConsumer(backend)
	firstDone := make(chan struct{})
	if err := consumer.Start(context.Background(), func(context.Context, Message) (ConsumeAction, error) {
		close(firstDone)
		return ConsumeAcknowledge, nil
	}); err != nil {
		t.Fatal(err)
	}
	if err := consumer.Start(context.Background(), func(context.Context, Message) (ConsumeAction, error) { return 0, nil }); !errors.Is(err, ErrAlreadyStarted) {
		t.Fatalf("expected already started, got %v", err)
	}
	<-firstDone
	if err := consumer.Stop(context.Background()); err != nil {
		t.Fatal(err)
	}
	if backend.ackCount.Load() != 1 {
		t.Fatalf("ack count=%d", backend.ackCount.Load())
	}

	backend.mu.Lock()
	backend.messages = append(backend.messages, NewBackendMessage(NewMessage([]byte("two")), "r2"))
	backend.mu.Unlock()
	backend.receive <- struct{}{}

	secondDone := make(chan struct{})
	if err := consumer.Start(context.Background(), func(context.Context, Message) (ConsumeAction, error) {
		close(secondDone)
		return ConsumeLeaveUnacknowledged, errors.New("handler failed")
	}); err != nil {
		t.Fatal(err)
	}
	<-secondDone
	err := consumer.Stop(context.Background())
	var mqErr *MQError
	if !errors.As(err, &mqErr) || mqErr.Code != ErrorHandler {
		t.Fatalf("expected handler error, got %v", err)
	}
	if backend.nackCount.Load() != 1 {
		t.Fatalf("nack count=%d", backend.nackCount.Load())
	}
	if err := consumer.Close(context.Background()); err != nil && !errors.As(err, &mqErr) {
		t.Fatal(err)
	}
	if err := consumer.Close(context.Background()); err != nil && !errors.As(err, &mqErr) {
		t.Fatal(err)
	}
	if backend.closeCount.Load() != 1 {
		t.Fatalf("close count=%d", backend.closeCount.Load())
	}
}
