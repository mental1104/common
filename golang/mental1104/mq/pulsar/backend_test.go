package pulsar

import (
	"context"
	"errors"
	"sync/atomic"
	"testing"
	"time"

	pulsargo "github.com/apache/pulsar-client-go/pulsar"
	commonmq "github.com/mental1104/common/golang/mental1104/mq"
)

type fakeMessageID struct {
	value     string
	partition int32
}

func (id fakeMessageID) Serialize() []byte   { return []byte(id.value) }
func (id fakeMessageID) LedgerID() int64     { return 1 }
func (id fakeMessageID) EntryID() int64      { return 2 }
func (id fakeMessageID) BatchIdx() int32     { return -1 }
func (id fakeMessageID) PartitionIdx() int32 { return id.partition }
func (id fakeMessageID) BatchSize() int32    { return 0 }
func (id fakeMessageID) String() string      { return id.value }

type fakeMessage struct {
	id      pulsargo.MessageID
	topic   string
	key     string
	payload []byte
	headers map[string]string
}

func (m fakeMessage) Topic() string                 { return m.topic }
func (m fakeMessage) Properties() map[string]string { return m.headers }
func (m fakeMessage) Payload() []byte               { return m.payload }
func (m fakeMessage) ID() pulsargo.MessageID        { return m.id }
func (m fakeMessage) Key() string                   { return m.key }

type fakeProducer struct {
	sendErr    error
	closeCount atomic.Int32
	flushCount atomic.Int32
}

func (p *fakeProducer) Send(context.Context, *pulsargo.ProducerMessage) (pulsargo.MessageID, error) {
	return fakeMessageID{value: "1:2", partition: 3}, p.sendErr
}
func (p *fakeProducer) SendAsync(_ context.Context, message *pulsargo.ProducerMessage, callback func(pulsargo.MessageID, *pulsargo.ProducerMessage, error)) {
	go callback(fakeMessageID{value: "1:2", partition: 3}, message, p.sendErr)
}
func (p *fakeProducer) FlushWithCtx(context.Context) error { p.flushCount.Add(1); return nil }
func (p *fakeProducer) Close()                             { p.closeCount.Add(1) }

type fakeConsumer struct {
	message     messageClient
	receiveErr  error
	ackCount    atomic.Int32
	nackCount   atomic.Int32
	closeCount  atomic.Int32
	unsubscribe atomic.Int32
}

func (c *fakeConsumer) Receive(context.Context) (messageClient, error) {
	return c.message, c.receiveErr
}
func (c *fakeConsumer) Ack(messageClient) error { c.ackCount.Add(1); return nil }
func (c *fakeConsumer) Nack(messageClient)      { c.nackCount.Add(1) }
func (c *fakeConsumer) Unsubscribe() error      { c.unsubscribe.Add(1); return nil }
func (c *fakeConsumer) Close()                  { c.closeCount.Add(1) }

type fakeClient struct {
	producer  producerClient
	consumers []consumerClient
	calls     atomic.Int32
	closed    atomic.Int32
}

func (c *fakeClient) CreateProducer(pulsargo.ProducerOptions) (producerClient, error) {
	return c.producer, nil
}
func (c *fakeClient) Subscribe(pulsargo.ConsumerOptions) (consumerClient, error) {
	index := int(c.calls.Add(1)) - 1
	if index >= len(c.consumers) {
		return nil, errors.New("no consumer")
	}
	return c.consumers[index], nil
}
func (c *fakeClient) Close() { c.closed.Add(1) }

func TestProducerBackendMapsMessageAndResult(t *testing.T) {
	native := &fakeProducer{}
	client := &fakeClient{producer: native}
	backend := newProducerBackend(client, native, time.Second)
	result, err := backend.Send(context.Background(), commonmq.Message{
		Key: []byte("key"), Payload: []byte("payload"), Headers: commonmq.MessageHeaders{"trace": "42"},
	})
	if err != nil || result.MessageID != "1:2" || result.Partition == nil || *result.Partition != 3 {
		t.Fatalf("result=%+v err=%v", result, err)
	}
	done := make(chan commonmq.SendResult, 1)
	if err := backend.SendAsync(context.Background(), commonmq.Message{}, func(result commonmq.SendResult) { done <- result }); err != nil {
		t.Fatal(err)
	}
	if result := <-done; result.MessageID != "1:2" || result.Err != nil {
		t.Fatalf("async result=%+v", result)
	}
	if err := backend.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
	if native.closeCount.Load() != 1 || native.flushCount.Load() != 1 || client.closed.Load() != 1 {
		t.Fatal("close lifecycle was not forwarded exactly once")
	}
}

func TestConsumerBackendConvertsMessageAndAckNack(t *testing.T) {
	id := fakeMessageID{value: "1:2", partition: 4}
	first := &fakeConsumer{message: fakeMessage{id: id, topic: "persistent://t/n/topic", key: "k", payload: []byte("v"), headers: map[string]string{"trace": "42"}}}
	second := &fakeConsumer{message: first.message}
	client := &fakeClient{consumers: []consumerClient{second}}
	backend := newConsumerBackend(client, first, pulsargo.ConsumerOptions{Topic: "persistent://t/n/topic", SubscriptionName: "sub"})

	received, err := backend.Receive(context.Background())
	if err != nil || received.Message.ID != "1:2" || received.ReceiptID == "" {
		t.Fatalf("received=%+v err=%v", received, err)
	}
	if err := backend.Acknowledge(context.Background(), received.ReceiptID); err != nil {
		t.Fatal(err)
	}
	if first.ackCount.Load() != 1 {
		t.Fatalf("ack count=%d", first.ackCount.Load())
	}

	received, _ = backend.Receive(context.Background())
	if err := backend.NegativeAcknowledge(context.Background(), received.ReceiptID); err != nil {
		t.Fatal(err)
	}
	if first.nackCount.Load() != 1 {
		t.Fatalf("nack count=%d", first.nackCount.Load())
	}
	if err := backend.Unsubscribe(context.Background()); err != nil {
		t.Fatal(err)
	}
	if err := backend.Resubscribe(context.Background()); err != nil {
		t.Fatal(err)
	}
	if err := backend.Close(context.Background()); err != nil {
		t.Fatal(err)
	}
	if client.closed.Load() != 1 {
		t.Fatal("client was not closed")
	}
}
