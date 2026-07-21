package kafka

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	kafkago "github.com/segmentio/kafka-go"
)

type Config struct {
	Brokers      []string
	WriterConfig func(*kafkago.Writer)
	ReaderConfig func(*kafkago.ReaderConfig)
}

type writer interface {
	WriteMessages(context.Context, ...kafkago.Message) error
	Close() error
}

type reader interface {
	FetchMessage(context.Context) (kafkago.Message, error)
	CommitMessages(context.Context, ...kafkago.Message) error
	Close() error
}

type writerFactory func(string, bool) (writer, error)
type readerFactory func(string, string) (reader, error)

type MessageQueue struct {
	mu            sync.Mutex
	closed        bool
	writerFactory writerFactory
	readerFactory readerFactory
}

func NewMessageQueue(config Config) (*MessageQueue, error) {
	if len(config.Brokers) == 0 {
		return nil, errors.New("kafka brokers must not be empty")
	}

	newWriter := func(topic string, batchingEnabled bool) (writer, error) {
		w := &kafkago.Writer{
			Addr:         kafkago.TCP(config.Brokers...),
			Topic:        topic,
			RequiredAcks: kafkago.RequireAll,
		}
		if !batchingEnabled {
			w.BatchSize = 1
			w.BatchTimeout = 0
		}
		if config.WriterConfig != nil {
			config.WriterConfig(w)
		}
		w.Async = false
		return w, nil
	}
	newReader := func(topic, subscription string) (reader, error) {
		cfg := kafkago.ReaderConfig{
			Brokers: config.Brokers,
			GroupID: subscription,
			Topic:   topic,
		}
		if config.ReaderConfig != nil {
			config.ReaderConfig(&cfg)
		}
		return kafkago.NewReader(cfg), nil
	}
	return newMessageQueue(newWriter, newReader), nil
}

func newMessageQueue(wf writerFactory, rf readerFactory) *MessageQueue {
	return &MessageQueue{writerFactory: wf, readerFactory: rf}
}

func (q *MessageQueue) CreateProducer(_ context.Context, tenant, namespace, topic string, _ commonmq.Schema, batchingEnabled bool) (commonmq.AbstractProducer, error) {
	fullTopic, err := commonmq.BuildKafkaTopic(tenant, namespace, topic)
	if err != nil {
		return nil, err
	}
	q.mu.Lock()
	defer q.mu.Unlock()
	if q.closed {
		return nil, commonmq.ErrClosed
	}
	native, err := q.writerFactory(fullTopic, batchingEnabled)
	if err != nil {
		return nil, fmt.Errorf("create kafka producer: %w", err)
	}
	return &Producer{writer: native, topic: fullTopic}, nil
}

func (q *MessageQueue) CreateConsumer(_ context.Context, tenant, namespace, topic, subscription string, _ commonmq.Schema, options commonmq.ConsumerOptions) (commonmq.AbstractConsumer, error) {
	fullTopic, err := commonmq.BuildKafkaTopic(tenant, namespace, topic)
	if err != nil {
		return nil, err
	}
	if subscription == "" {
		return nil, errors.New("kafka subscription must not be empty")
	}
	q.mu.Lock()
	defer q.mu.Unlock()
	if q.closed {
		return nil, commonmq.ErrClosed
	}
	native, err := q.readerFactory(fullTopic, subscription)
	if err != nil {
		return nil, fmt.Errorf("create kafka consumer: %w", err)
	}
	return &Consumer{
		reader:        native,
		readerFactory: q.readerFactory,
		topic:         fullTopic,
		subscription:  subscription,
		listener:      options.MessageListener,
	}, nil
}

func (q *MessageQueue) Close() error {
	q.mu.Lock()
	q.closed = true
	q.mu.Unlock()
	return nil
}

type Producer struct {
	mu     sync.Mutex
	closed bool
	wg     sync.WaitGroup
	writer writer
	topic  string
}

func (p *Producer) begin() error {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.closed {
		return commonmq.ErrClosed
	}
	p.wg.Add(1)
	return nil
}

func (p *Producer) Send(ctx context.Context, record any) error {
	if err := p.begin(); err != nil {
		return err
	}
	defer p.wg.Done()
	payload, err := commonmq.MarshalRecord(record)
	if err != nil {
		return err
	}
	if err := p.writer.WriteMessages(ctx, kafkago.Message{Value: payload}); err != nil {
		return fmt.Errorf("kafka send: %w", err)
	}
	return nil
}

func (p *Producer) SendAsync(ctx context.Context, record any, callback commonmq.SendCallback) error {
	if err := p.begin(); err != nil {
		return err
	}
	payload, err := commonmq.MarshalRecord(record)
	if err != nil {
		p.wg.Done()
		return err
	}
	payload = append([]byte(nil), payload...)
	go func() {
		defer p.wg.Done()
		err := p.writer.WriteMessages(ctx, kafkago.Message{Value: payload})
		if err != nil {
			err = fmt.Errorf("kafka async send: %w", err)
		}
		if callback != nil {
			callback(commonmq.SendResult{Err: err})
		}
	}()
	return nil
}

func (p *Producer) Close() error {
	p.mu.Lock()
	if p.closed {
		p.mu.Unlock()
		return nil
	}
	p.closed = true
	p.mu.Unlock()
	p.wg.Wait()
	return p.writer.Close()
}

type Consumer struct {
	opMu          sync.Mutex
	closed        bool
	reader        reader
	readerFactory readerFactory
	topic         string
	subscription  string
	listener      commonmq.MessageListener
}

func (c *Consumer) Receive(ctx context.Context, timeout time.Duration) (*commonmq.Message, error) {
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed || c.reader == nil {
		return nil, commonmq.ErrClosed
	}
	if timeout > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, timeout)
		defer cancel()
	}
	native, err := c.reader.FetchMessage(ctx)
	if err != nil {
		if errors.Is(err, context.DeadlineExceeded) {
			return nil, commonmq.ErrTimeout
		}
		return nil, fmt.Errorf("kafka receive: %w", err)
	}
	message := &commonmq.Message{Payload: append([]byte(nil), native.Value...), Native: native}
	if c.listener != nil {
		c.listener(message)
	}
	return message, nil
}

func kafkaMessage(message *commonmq.Message) (kafkago.Message, error) {
	if message == nil {
		return kafkago.Message{}, commonmq.ErrInvalidMessage
	}
	native, ok := message.Native.(kafkago.Message)
	if !ok {
		return kafkago.Message{}, commonmq.ErrInvalidMessage
	}
	return native, nil
}

func (c *Consumer) Acknowledge(ctx context.Context, message *commonmq.Message) error {
	native, err := kafkaMessage(message)
	if err != nil {
		return err
	}
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed || c.reader == nil {
		return commonmq.ErrClosed
	}
	if err := c.reader.CommitMessages(ctx, native); err != nil {
		return fmt.Errorf("kafka acknowledge: %w", err)
	}
	return nil
}

func (c *Consumer) NegativeAcknowledge(_ context.Context, message *commonmq.Message) error {
	if _, err := kafkaMessage(message); err != nil {
		return err
	}
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed || c.reader == nil {
		return commonmq.ErrClosed
	}
	if err := c.reader.Close(); err != nil {
		return fmt.Errorf("kafka negative acknowledge close: %w", err)
	}
	next, err := c.readerFactory(c.topic, c.subscription)
	if err != nil {
		c.reader = nil
		return fmt.Errorf("kafka negative acknowledge resubscribe: %w", err)
	}
	c.reader = next
	return nil
}

func (c *Consumer) Unsubscribe(_ context.Context) error {
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed {
		return commonmq.ErrClosed
	}
	if c.reader == nil {
		return nil
	}
	err := c.reader.Close()
	c.reader = nil
	return err
}

func (c *Consumer) Resubscribe(_ context.Context) error {
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed {
		return commonmq.ErrClosed
	}
	if c.reader != nil {
		if err := c.reader.Close(); err != nil {
			return err
		}
	}
	next, err := c.readerFactory(c.topic, c.subscription)
	if err != nil {
		c.reader = nil
		return fmt.Errorf("kafka resubscribe: %w", err)
	}
	c.reader = next
	return nil
}

func (c *Consumer) Close() error {
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed {
		return nil
	}
	c.closed = true
	if c.reader == nil {
		return nil
	}
	err := c.reader.Close()
	c.reader = nil
	return err
}
