package pulsar

import (
	"context"
	"errors"
	"sync/atomic"
	"testing"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	pulsargo "github.com/apache/pulsar-client-go/pulsar"
)

type fakeProducer struct {
	sendErr error
	closed  atomic.Int32
	flushes atomic.Int32
}

func (p *fakeProducer) Send(context.Context, *pulsargo.ProducerMessage) (pulsargo.MessageID, error) {
	if p.sendErr != nil {
		return nil, p.sendErr
	}
	return pulsargo.NewMessageID(1, 2, 0, 0), nil
}
func (p *fakeProducer) SendAsync(_ context.Context, message *pulsargo.ProducerMessage, callback func(pulsargo.MessageID, *pulsargo.ProducerMessage, error)) {
	go callback(pulsargo.NewMessageID(1, 2, 0, 0), message, p.sendErr)
}
func (p *fakeProducer) FlushWithCtx(context.Context) error { p.flushes.Add(1); return nil }
func (p *fakeProducer) Close()                              { p.closed.Add(1) }

type fakeMessage struct{ payload []byte }

func (m fakeMessage) Topic() string                                  { return "persistent://t/n/topic" }
func (m fakeMessage) ProducerName() string                           { return "producer" }
func (m fakeMessage) Properties() map[string]string                  { return nil }
func (m fakeMessage) Payload() []byte                                { return m.payload }
func (m fakeMessage) IsNullValue() bool                              { return false }
func (m fakeMessage) ID() pulsargo.MessageID                         { return pulsargo.NewMessageID(1, 2, 0, 0) }
func (m fakeMessage) PublishTime() time.Time                         { return time.Time{} }
func (m fakeMessage) EventTime() time.Time                           { return time.Time{} }
func (m fakeMessage) Key() string                                    { return "" }
func (m fakeMessage) OrderingKey() string                            { return "" }
func (m fakeMessage) RedeliveryCount() uint32                        { return 0 }
func (m fakeMessage) IsReplicated() bool                             { return false }
func (m fakeMessage) GetReplicatedFrom() string                      { return "" }
func (m fakeMessage) GetSchemaValue(interface{}) error               { return nil }
func (m fakeMessage) SchemaVersion() []byte                          { return nil }
func (m fakeMessage) GetEncryptionContext() *pulsargo.EncryptionContext { return nil }
func (m fakeMessage) Index() *uint64                                 { return nil }
func (m fakeMessage) BrokerPublishTime() *time.Time                  { return nil }

type fakeConsumer struct {
	message      pulsargo.Message
	receiveErr   error
	acks         atomic.Int32
	nacks        atomic.Int32
	unsubscribes atomic.Int32
	closed       atomic.Int32
}

func (c *fakeConsumer) Receive(context.Context) (pulsargo.Message, error) {
	return c.message, c.receiveErr
}
func (c *fakeConsumer) Ack(pulsargo.Message) error { c.acks.Add(1); return nil }
func (c *fakeConsumer) Nack(pulsargo.Message)      { c.nacks.Add(1) }
func (c *fakeConsumer) Unsubscribe() error         { c.unsubscribes.Add(1); return nil }
func (c *fakeConsumer) Close()                     { c.closed.Add(1) }

type fakeClient struct {
	producer       producerClient
	consumers      []consumerClient
	subscribeCalls atomic.Int32
}

func (c *fakeClient) CreateProducer(pulsargo.ProducerOptions) (producerClient, error) {
	return c.producer, nil
}
func (c *fakeClient) Subscribe(pulsargo.ConsumerOptions) (consumerClient, error) {
	index := int(c.subscribeCalls.Add(1)) - 1
	if index >= len(c.consumers) {
		return nil, errors.New("no consumer")
	}
	return c.consumers[index], nil
}
func (c *fakeClient) Close() {}

func TestProducerAsyncCallbacksSuccessAndFailureOnce(t *testing.T) {
	for _, sendErr := range []error{nil, errors.New("broker unavailable")} {
		native := &fakeProducer{sendErr: sendErr}
		q := newMessageQueue(&fakeClient{producer: native}, Config{CloseTimeout: time.Second})
		producer, err := q.CreateProducer(context.Background(), "t", "n", "topic", nil, true)
		if err != nil {
			t.Fatal(err)
		}
		var count atomic.Int32
		done := make(chan commonmq.SendResult, 1)
		if err := producer.SendAsync(context.Background(), "message", func(r commonmq.SendResult) {
			count.Add(1)
			done <- r
		}); err != nil {
			t.Fatal(err)
		}
		got := <-done
		if (sendErr == nil) != (got.Err == nil) {
			t.Fatalf("sendErr=%v callback=%+v", sendErr, got)
		}
		if sendErr == nil && got.MessageID == "" {
			t.Fatal("successful send should expose message id")
		}
		if err := producer.Close(); err != nil {
			t.Fatal(err)
		}
		if err := producer.Close(); err != nil {
			t.Fatal(err)
		}
		if count.Load() != 1 || native.closed.Load() != 1 || native.flushes.Load() != 1 {
			t.Fatal("callback/close must be exactly once")
		}
	}
}

func TestProducerCallbackMayCloseProducer(t *testing.T) {
	native := &fakeProducer{}
	q := newMessageQueue(&fakeClient{producer: native}, Config{CloseTimeout: time.Second})
	producer, err := q.CreateProducer(context.Background(), "t", "n", "topic", nil, true)
	if err != nil {
		t.Fatal(err)
	}
	done := make(chan error, 1)
	if err := producer.SendAsync(context.Background(), "message", func(commonmq.SendResult) {
		done <- producer.Close()
	}); err != nil {
		t.Fatal(err)
	}
	select {
	case err := <-done:
		if err != nil {
			t.Fatal(err)
		}
	case <-time.After(time.Second):
		t.Fatal("callback deadlocked while closing producer")
	}
	if native.closed.Load() != 1 || native.flushes.Load() != 1 {
		t.Fatal("producer close was not forwarded exactly once")
	}
}

func TestConsumerAckNackUnsubscribeResubscribeAndTimeout(t *testing.T) {
	first := &fakeConsumer{message: fakeMessage{payload: []byte("one")}}
	second := &fakeConsumer{message: fakeMessage{payload: []byte("two")}}
	q := newMessageQueue(&fakeClient{consumers: []consumerClient{first, second}}, Config{})
	consumer, err := q.CreateConsumer(context.Background(), "t", "n", "topic", "sub", nil, commonmq.ConsumerOptions{})
	if err != nil {
		t.Fatal(err)
	}
	message, err := consumer.Receive(context.Background(), time.Second)
	if err != nil || string(message.Payload) != "one" {
		t.Fatalf("payload=%q err=%v", message.Payload, err)
	}
	if err := consumer.Acknowledge(context.Background(), message); err != nil {
		t.Fatal(err)
	}
	if err := consumer.NegativeAcknowledge(context.Background(), message); err != nil {
		t.Fatal(err)
	}
	if err := consumer.Unsubscribe(context.Background()); err != nil {
		t.Fatal(err)
	}
	if err := consumer.Resubscribe(context.Background()); err != nil {
		t.Fatal(err)
	}
	if first.acks.Load() != 1 || first.nacks.Load() != 1 || first.unsubscribes.Load() != 1 {
		t.Fatal("consumer operations were not forwarded")
	}
	if err := consumer.Close(); err != nil {
		t.Fatal(err)
	}
	if err := consumer.Close(); err != nil {
		t.Fatal(err)
	}

	timeout := &fakeConsumer{receiveErr: context.DeadlineExceeded}
	timeoutQueue := newMessageQueue(&fakeClient{consumers: []consumerClient{timeout}}, Config{})
	timeoutConsumer, _ := timeoutQueue.CreateConsumer(context.Background(), "t", "n", "topic", "sub", nil, commonmq.ConsumerOptions{})
	if _, err := timeoutConsumer.Receive(context.Background(), time.Millisecond); !errors.Is(err, commonmq.ErrTimeout) {
		t.Fatalf("expected timeout, got %v", err)
	}
}

func TestConsumerListenerMayAcknowledge(t *testing.T) {
	native := &fakeConsumer{message: fakeMessage{payload: []byte("one")}}
	q := newMessageQueue(&fakeClient{consumers: []consumerClient{native}}, Config{})
	var consumer commonmq.AbstractConsumer
	done := make(chan error, 1)
	var err error
	consumer, err = q.CreateConsumer(
		context.Background(), "t", "n", "topic", "sub", nil,
		commonmq.ConsumerOptions{MessageListener: func(message *commonmq.Message) {
			done <- consumer.Acknowledge(context.Background(), message)
		}},
	)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := consumer.Receive(context.Background(), time.Second); err != nil {
		t.Fatal(err)
	}
	select {
	case err := <-done:
		if err != nil {
			t.Fatal(err)
		}
	case <-time.After(time.Second):
		t.Fatal("listener deadlocked while acknowledging message")
	}
	if native.acks.Load() != 1 {
		t.Fatalf("ack count=%d", native.acks.Load())
	}
}
