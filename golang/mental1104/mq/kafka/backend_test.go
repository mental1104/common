package kafka

import (
	"context"
	"errors"
	"sync/atomic"
	"testing"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	kafkago "github.com/segmentio/kafka-go"
)

type fakeWriter struct {
	messages   []kafkago.Message
	writeErr   error
	closeCount atomic.Int32
}

func (w *fakeWriter) WriteMessages(_ context.Context, messages ...kafkago.Message) error {
	w.messages = append(w.messages, messages...)
	return w.writeErr
}
func (w *fakeWriter) Close() error { w.closeCount.Add(1); return nil }

type fakeReader struct {
	message    kafkago.Message
	fetchErr   error
	commits    atomic.Int32
	closeCount atomic.Int32
}

func (r *fakeReader) FetchMessage(context.Context) (kafkago.Message, error) {
	return r.message, r.fetchErr
}
func (r *fakeReader) CommitMessages(context.Context, ...kafkago.Message) error {
	r.commits.Add(1)
	return nil
}
func (r *fakeReader) Close() error { r.closeCount.Add(1); return nil }

func TestProducerBackendConvertsDomainMessage(t *testing.T) {
	native := &fakeWriter{}
	backend := newProducerBackend(native)
	partition := 2
	result, err := backend.Send(context.Background(), commonmq.Message{
		Key:       []byte("key"),
		Payload:   []byte("payload"),
		Headers:   commonmq.MessageHeaders{"trace": "42"},
		Partition: &partition,
	})
	if err != nil || result.Partition == nil || *result.Partition != 2 {
		t.Fatalf("result=%+v err=%v", result, err)
	}
	if len(native.messages) != 1 || string(native.messages[0].Value) != "payload" || string(native.messages[0].Key) != "key" {
		t.Fatalf("native message=%+v", native.messages)
	}
	if len(native.messages[0].Headers) != 1 || native.messages[0].Headers[0].Key != "trace" {
		t.Fatalf("headers=%+v", native.messages[0].Headers)
	}
}

func TestProducerBackendAsyncFailureAndClose(t *testing.T) {
	nativeErr := errors.New("broker unavailable")
	native := &fakeWriter{writeErr: nativeErr}
	backend := newProducerBackend(native)
	done := make(chan commonmq.SendResult, 1)
	if err := backend.SendAsync(context.Background(), commonmq.Message{}, func(result commonmq.SendResult) {
		done <- result
	}); err != nil {
		t.Fatal(err)
	}
	result := <-done
	if result.Err == nil || !errors.Is(result.Err, nativeErr) {
		t.Fatalf("result=%+v", result)
	}
	if err := backend.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
	if err := backend.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
	if native.closeCount.Load() != 1 {
		t.Fatalf("close count=%d", native.closeCount.Load())
	}
}

func TestConsumerBackendHidesNativeMessageAndForwardsAckNack(t *testing.T) {
	first := &fakeReader{message: kafkago.Message{
		Topic: "tenant.namespace.topic", Partition: 1, Offset: 7,
		Key: []byte("k"), Value: []byte("v"),
		Headers: []kafkago.Header{{Key: "trace", Value: []byte("42")}},
	}}
	second := &fakeReader{message: first.message}
	readers := []reader{first, second}
	factory := func() (reader, error) {
		if len(readers) == 0 {
			return nil, errors.New("no reader")
		}
		result := readers[0]
		readers = readers[1:]
		return result, nil
	}
	backend := newConsumerBackend("tenant.namespace.topic", "sub", factory, first)
	received, err := backend.Receive(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if received.ReceiptID == "" || received.Message.ID != "tenant.namespace.topic/1/7" {
		t.Fatalf("received=%+v", received)
	}
	if err := backend.Acknowledge(context.Background(), received.ReceiptID); err != nil {
		t.Fatal(err)
	}
	if first.commits.Load() != 1 {
		t.Fatalf("commit count=%d", first.commits.Load())
	}

	received, err = backend.Receive(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if err := backend.NegativeAcknowledge(context.Background(), received.ReceiptID); err != nil {
		t.Fatal(err)
	}
	if first.closeCount.Load() != 1 {
		t.Fatalf("close count=%d", first.closeCount.Load())
	}
	if err := backend.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
}

func TestConsumerBackendReturnsContextError(t *testing.T) {
	native := &fakeReader{fetchErr: context.DeadlineExceeded}
	backend := newConsumerBackend("topic", "sub", func() (reader, error) { return native, nil }, native)
	_, err := backend.Receive(context.Background())
	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("err=%v", err)
	}
	_ = backend.Close(context.Background())
	_ = time.Second
}
