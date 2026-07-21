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
	writeErr error
	closed   atomic.Int32
}

func (w *fakeWriter) WriteMessages(context.Context, ...kafkago.Message) error { return w.writeErr }
func (w *fakeWriter) Close() error { w.closed.Add(1); return nil }

type fakeReader struct {
	message  kafkago.Message
	fetchErr error
	commits  atomic.Int32
	closed   atomic.Int32
}

func (r *fakeReader) FetchMessage(context.Context) (kafkago.Message, error) { return r.message, r.fetchErr }
func (r *fakeReader) CommitMessages(context.Context, ...kafkago.Message) error { r.commits.Add(1); return nil }
func (r *fakeReader) Close() error { r.closed.Add(1); return nil }

func TestProducerAsyncCallbacksSuccessAndFailureOnce(t *testing.T) {
	for _, sendErr := range []error{nil, errors.New("broker unavailable")} {
		w := &fakeWriter{writeErr: sendErr}
		q := newMessageQueue(func(string, bool) (writer, error) { return w, nil }, nil)
		producer, err := q.CreateProducer(context.Background(), "t", "n", "topic", nil, true)
		if err != nil { t.Fatal(err) }
		var count atomic.Int32
		done := make(chan error, 1)
		if err := producer.SendAsync(context.Background(), "message", func(r commonmq.SendResult) {
			count.Add(1)
			done <- r.Err
		}); err != nil { t.Fatal(err) }
		got := <-done
		if (sendErr == nil) != (got == nil) { t.Fatalf("sendErr=%v callbackErr=%v", sendErr, got) }
		if err := producer.Close(); err != nil { t.Fatal(err) }
		if err := producer.Close(); err != nil { t.Fatal(err) }
		if count.Load() != 1 || w.closed.Load() != 1 { t.Fatalf("callback=%d close=%d", count.Load(), w.closed.Load()) }
		if !errors.Is(producer.Send(context.Background(), "late"), commonmq.ErrClosed) { t.Fatal("send after close should fail") }
	}
}

func TestConsumerReceiveAckNackAndTimeout(t *testing.T) {
	first := &fakeReader{message: kafkago.Message{Value: []byte("one")}}
	second := &fakeReader{message: kafkago.Message{Value: []byte("two")}}
	readers := []reader{first, second}
	calls := 0
	q := newMessageQueue(nil, func(string, string) (reader, error) {
		if calls >= len(readers) { return nil, errors.New("no reader") }
		r := readers[calls]
		calls++
		return r, nil
	})
	consumer, err := q.CreateConsumer(context.Background(), "t", "n", "topic", "sub", nil, commonmq.ConsumerOptions{})
	if err != nil { t.Fatal(err) }
	message, err := consumer.Receive(context.Background(), time.Second)
	if err != nil || string(message.Payload) != "one" { t.Fatalf("payload=%q err=%v", message.Payload, err) }
	if err := consumer.Acknowledge(context.Background(), message); err != nil { t.Fatal(err) }
	if err := consumer.NegativeAcknowledge(context.Background(), message); err != nil { t.Fatal(err) }
	if first.commits.Load() != 1 || first.closed.Load() != 1 { t.Fatal("ack/nack not forwarded") }
	message, err = consumer.Receive(context.Background(), time.Second)
	if err != nil || string(message.Payload) != "two" { t.Fatalf("payload=%q err=%v", message.Payload, err) }
	if err := consumer.Close(); err != nil { t.Fatal(err) }
	if err := consumer.Close(); err != nil { t.Fatal(err) }

	timeoutReader := &fakeReader{fetchErr: context.DeadlineExceeded}
	timeoutQueue := newMessageQueue(nil, func(string, string) (reader, error) { return timeoutReader, nil })
	timeoutConsumer, _ := timeoutQueue.CreateConsumer(context.Background(), "t", "n", "topic", "sub", nil, commonmq.ConsumerOptions{})
	if _, err := timeoutConsumer.Receive(context.Background(), time.Millisecond); !errors.Is(err, commonmq.ErrTimeout) {
		t.Fatalf("expected timeout, got %v", err)
	}
}
