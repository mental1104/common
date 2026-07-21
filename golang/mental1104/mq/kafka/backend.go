package kafka

import (
	"context"
	"fmt"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	kafkago "github.com/segmentio/kafka-go"
)

type AckMode int

const (
	AckAll AckMode = iota
	AckLeader
	AckNone
)

type Config struct {
	Brokers       []string
	DialTimeout   time.Duration
	ReadTimeout   time.Duration
	WriteTimeout  time.Duration
	BatchSize     int
	BatchBytes    int64
	BatchTimeout  time.Duration
	QueueCapacity int
	MinBytes      int
	MaxBytes      int
	MaxWait       time.Duration
	AckMode       AckMode
}

func (Config) BackendType() commonmq.BackendType { return commonmq.BackendKafka }

type writer interface {
	WriteMessages(context.Context, ...kafkago.Message) error
	Close() error
}

type reader interface {
	FetchMessage(context.Context) (kafkago.Message, error)
	CommitMessages(context.Context, ...kafkago.Message) error
	Close() error
}

type readerFactory func() (reader, error)

type operationTracker struct {
	mu      sync.Mutex
	closed  bool
	pending int
	idle    chan struct{}
}

func newOperationTracker() operationTracker {
	idle := make(chan struct{})
	close(idle)
	return operationTracker{idle: idle}
}

func (t *operationTracker) begin() error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.closed {
		return commonmq.ErrClosed
	}
	if t.pending == 0 {
		t.idle = make(chan struct{})
	}
	t.pending++
	return nil
}

func (t *operationTracker) finish() {
	t.mu.Lock()
	t.pending--
	if t.pending == 0 {
		close(t.idle)
	}
	t.mu.Unlock()
}

func (t *operationTracker) closeAndIdle() <-chan struct{} {
	t.mu.Lock()
	t.closed = true
	idle := t.idle
	t.mu.Unlock()
	return idle
}

func requiredAcks(mode AckMode) kafkago.RequiredAcks {
	switch mode {
	case AckNone:
		return kafkago.RequireNone
	case AckLeader:
		return kafkago.RequireOne
	default:
		return kafkago.RequireAll
	}
}

func newWriter(config Config, topic string, disableBatching bool) writer {
	result := &kafkago.Writer{
		Addr: kafkago.TCP(config.Brokers...), Topic: topic,
		RequiredAcks: requiredAcks(config.AckMode), ReadTimeout: config.ReadTimeout,
		WriteTimeout: config.WriteTimeout, BatchSize: config.BatchSize,
		BatchBytes: config.BatchBytes, BatchTimeout: config.BatchTimeout, Async: false,
	}
	if disableBatching {
		result.BatchSize = 1
		result.BatchTimeout = 0
	}
	return result
}

func newReader(config Config, topic, subscription string) readerFactory {
	return func() (reader, error) {
		cfg := kafkago.ReaderConfig{
			Brokers: append([]string(nil), config.Brokers...), GroupID: subscription,
			Topic: topic, QueueCapacity: config.QueueCapacity, MinBytes: config.MinBytes,
			MaxBytes: config.MaxBytes, MaxWait: config.MaxWait, CommitInterval: 0,
		}
		if config.DialTimeout > 0 {
			cfg.Dialer = &kafkago.Dialer{Timeout: config.DialTimeout}
		}
		return kafkago.NewReader(cfg), nil
	}
}

type producerBackend struct {
	writer writer
	tracker operationTracker
	closeOnce sync.Once
	closeDone chan struct{}
	closeErr error
}

func NewProducerBackend(config commonmq.ProducerConfig, backend Config) (commonmq.ProducerBackend, error) {
	if len(backend.Brokers) == 0 {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create kafka producer backend", commonmq.BackendKafka, "brokers must not be empty", nil)
	}
	topic, err := commonmq.BuildKafkaTopic(config.Topic)
	if err != nil { return nil, err }
	return newProducerBackend(newWriter(backend, topic, config.DisableBatching)), nil
}

func newProducerBackend(native writer) *producerBackend {
	return &producerBackend{writer: native, tracker: newOperationTracker(), closeDone: make(chan struct{})}
}

func kafkaMessage(message commonmq.Message) kafkago.Message {
	headers := make([]kafkago.Header, 0, len(message.Headers))
	for key, value := range message.Headers { headers = append(headers, kafkago.Header{Key: key, Value: []byte(value)}) }
	native := kafkago.Message{Key: append([]byte(nil), message.Key...), Value: append([]byte(nil), message.Payload...), Headers: headers}
	if message.Partition != nil { native.Partition = *message.Partition }
	return native
}

func (b *producerBackend) Send(ctx context.Context, message commonmq.Message) (commonmq.SendResult, error) {
	if err := b.tracker.begin(); err != nil { return commonmq.SendResult{Err: err}, err }
	defer b.tracker.finish()
	if err := b.writer.WriteMessages(ctx, kafkaMessage(message)); err != nil {
		err = commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka send", commonmq.BackendKafka)
		return commonmq.SendResult{Err: err}, err
	}
	result := commonmq.SendResult{}
	if message.Partition != nil { partition := *message.Partition; result.Partition = &partition }
	return result, nil
}

func (b *producerBackend) SendAsync(ctx context.Context, message commonmq.Message, callback commonmq.DeliveryCallback) error {
	if err := b.tracker.begin(); err != nil { return err }
	native := kafkaMessage(message)
	go func() {
		err := b.writer.WriteMessages(ctx, native)
		if err != nil { err = commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka async send", commonmq.BackendKafka) }
		b.tracker.finish()
		if callback != nil {
			result := commonmq.SendResult{Partition: message.Partition, Err: err}
			go func() { defer func(){ _ = recover() }(); callback(result) }()
		}
	}()
	return nil
}

func (b *producerBackend) Close(ctx context.Context) error {
	b.closeOnce.Do(func() { go func(){ idle := b.tracker.closeAndIdle(); <-idle; b.closeErr = commonmq.NormalizeError(b.writer.Close(), commonmq.ErrorBackend, "close kafka producer", commonmq.BackendKafka); close(b.closeDone) }() })
	select {
	case <-b.closeDone: return b.closeErr
	case <-ctx.Done(): return commonmq.NormalizeError(ctx.Err(), commonmq.ErrorCanceled, "close kafka producer", commonmq.BackendKafka)
	}
}

type consumerBackend struct {
	topic string
	subscription string
	readerFactory readerFactory
	mu sync.Mutex
	closed bool
	reader reader
	receipts map[string]kafkago.Message
	sequence atomic.Uint64
}

func NewConsumerBackend(config commonmq.ConsumerConfig, backend Config) (commonmq.ConsumerBackend, error) {
	if len(backend.Brokers) == 0 { return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create kafka consumer backend", commonmq.BackendKafka, "brokers must not be empty", nil) }
	if config.Subscription == "" { return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create kafka consumer backend", commonmq.BackendKafka, "subscription must not be empty", nil) }
	topic, err := commonmq.BuildKafkaTopic(config.Topic); if err != nil { return nil, err }
	factory := newReader(backend, topic, config.Subscription); native, err := factory()
	if err != nil { return nil, commonmq.NormalizeError(err, commonmq.ErrorBackend, "create kafka reader", commonmq.BackendKafka) }
	return newConsumerBackend(topic, config.Subscription, factory, native), nil
}

func newConsumerBackend(topic, subscription string, factory readerFactory, native reader) *consumerBackend {
	return &consumerBackend{topic: topic, subscription: subscription, readerFactory: factory, reader: native, receipts: make(map[string]kafkago.Message)}
}

func (b *consumerBackend) Receive(ctx context.Context) (commonmq.BackendMessage, error) {
	b.mu.Lock(); if b.closed || b.reader == nil { b.mu.Unlock(); return commonmq.BackendMessage{}, commonmq.ErrClosed }; native := b.reader; b.mu.Unlock()
	message, err := native.FetchMessage(ctx); if err != nil { return commonmq.BackendMessage{}, err }
	receiptID := fmt.Sprintf("%s:%d:%d:%d", message.Topic, message.Partition, message.Offset, b.sequence.Add(1))
	headers := make(commonmq.MessageHeaders, len(message.Headers)); for _, h := range message.Headers { headers[h.Key] = string(h.Value) }
	partition := message.Partition
	domain := commonmq.Message{Topic: message.Topic, Key: append([]byte(nil), message.Key...), Payload: append([]byte(nil), message.Value...), Headers: headers, Partition: &partition, ID: message.Topic+"/"+strconv.Itoa(message.Partition)+"/"+strconv.FormatInt(message.Offset,10)}
	b.mu.Lock(); if b.closed { b.mu.Unlock(); return commonmq.BackendMessage{}, commonmq.ErrClosed }; b.receipts[receiptID] = message; b.mu.Unlock()
	return commonmq.NewBackendMessage(domain, receiptID), nil
}

func (b *consumerBackend) Acknowledge(ctx context.Context, receiptID string) error {
	b.mu.Lock(); if b.closed || b.reader == nil { b.mu.Unlock(); return commonmq.ErrClosed }; message, ok := b.receipts[receiptID]; native := b.reader; b.mu.Unlock()
	if !ok { return commonmq.ErrInvalidMessage }
	if err := native.CommitMessages(ctx, message); err != nil { return commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka acknowledge", commonmq.BackendKafka) }
	b.mu.Lock(); delete(b.receipts, receiptID); b.mu.Unlock(); return nil
}

func (b *consumerBackend) NegativeAcknowledge(context.Context, string) error {
	b.mu.Lock(); if b.closed || b.reader == nil { b.mu.Unlock(); return commonmq.ErrClosed }; current := b.reader; b.mu.Unlock()
	if err := current.Close(); err != nil { return commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka negative acknowledge close", commonmq.BackendKafka) }
	next, err := b.readerFactory(); if err != nil { return commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka negative acknowledge resubscribe", commonmq.BackendKafka) }
	b.mu.Lock(); b.reader = next; b.receipts = make(map[string]kafkago.Message); b.mu.Unlock(); return nil
}

func (b *consumerBackend) Unsubscribe(context.Context) error {
	b.mu.Lock(); if b.closed { b.mu.Unlock(); return commonmq.ErrClosed }; current := b.reader; b.reader = nil; b.receipts = make(map[string]kafkago.Message); b.mu.Unlock()
	if current == nil { return nil }; return commonmq.NormalizeError(current.Close(), commonmq.ErrorBackend, "unsubscribe kafka consumer", commonmq.BackendKafka)
}

func (b *consumerBackend) Resubscribe(context.Context) error {
	b.mu.Lock(); if b.closed { b.mu.Unlock(); return commonmq.ErrClosed }; current := b.reader; b.mu.Unlock()
	if current != nil { if err := current.Close(); err != nil { return commonmq.NormalizeError(err, commonmq.ErrorBackend, "close kafka reader before resubscribe", commonmq.BackendKafka) } }
	next, err := b.readerFactory(); if err != nil { return commonmq.NormalizeError(err, commonmq.ErrorBackend, "resubscribe kafka consumer", commonmq.BackendKafka) }
	b.mu.Lock(); b.reader = next; b.receipts = make(map[string]kafkago.Message); b.mu.Unlock(); return nil
}

func (b *consumerBackend) Close(context.Context) error {
	b.mu.Lock(); if b.closed { b.mu.Unlock(); return nil }; b.closed = true; current := b.reader; b.reader = nil; b.receipts = nil; b.mu.Unlock()
	if current == nil { return nil }; return commonmq.NormalizeError(current.Close(), commonmq.ErrorBackend, "close kafka consumer", commonmq.BackendKafka)
}

var _ commonmq.ProducerBackend = (*producerBackend)(nil)
var _ commonmq.ConsumerBackend = (*consumerBackend)(nil)
