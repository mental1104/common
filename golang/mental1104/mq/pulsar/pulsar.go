package pulsar

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	pulsargo "github.com/apache/pulsar-client-go/pulsar"
)

type Config struct {
	ClientOptions  pulsargo.ClientOptions
	ProducerConfig func(*pulsargo.ProducerOptions)
	ConsumerConfig func(*pulsargo.ConsumerOptions)
	CloseTimeout   time.Duration
}

type client interface {
	CreateProducer(pulsargo.ProducerOptions) (producerClient, error)
	Subscribe(pulsargo.ConsumerOptions) (consumerClient, error)
	Close()
}

type nativeClient struct{ pulsargo.Client }

func (c nativeClient) CreateProducer(options pulsargo.ProducerOptions) (producerClient, error) {
	return c.Client.CreateProducer(options)
}
func (c nativeClient) Subscribe(options pulsargo.ConsumerOptions) (consumerClient, error) {
	return c.Client.Subscribe(options)
}

type producerClient interface {
	Send(context.Context, *pulsargo.ProducerMessage) (pulsargo.MessageID, error)
	SendAsync(context.Context, *pulsargo.ProducerMessage, func(pulsargo.MessageID, *pulsargo.ProducerMessage, error))
	FlushWithCtx(context.Context) error
	Close()
}

type consumerClient interface {
	Receive(context.Context) (pulsargo.Message, error)
	Ack(pulsargo.Message) error
	Nack(pulsargo.Message)
	Unsubscribe() error
	Close()
}

type MessageQueue struct {
	mu             sync.Mutex
	closed         bool
	client         client
	ownsClient     bool
	producerConfig func(*pulsargo.ProducerOptions)
	consumerConfig func(*pulsargo.ConsumerOptions)
	closeTimeout   time.Duration
}

type PulsarMessageQueue = MessageQueue
type PulsarProducer = Producer
type PulsarConsumer = Consumer

func NewMessageQueue(config Config) (*MessageQueue, error) {
	if config.ClientOptions.URL == "" {
		return nil, errors.New("pulsar client URL must not be empty")
	}
	native, err := pulsargo.NewClient(config.ClientOptions)
	if err != nil {
		return nil, fmt.Errorf("create pulsar client: %w", err)
	}
	q := newMessageQueue(nativeClient{Client: native}, config)
	q.ownsClient = true
	return q, nil
}

func newMessageQueue(native client, config Config) *MessageQueue {
	timeout := config.CloseTimeout
	if timeout <= 0 {
		timeout = 10 * time.Second
	}
	return &MessageQueue{
		client:         native,
		producerConfig: config.ProducerConfig,
		consumerConfig: config.ConsumerConfig,
		closeTimeout:   timeout,
	}
}

func (q *MessageQueue) CreateProducer(_ context.Context, tenant, namespace, topic string, _ commonmq.Schema, batchingEnabled bool) (commonmq.AbstractProducer, error) {
	fullTopic, err := commonmq.BuildPulsarTopic(tenant, namespace, topic)
	if err != nil {
		return nil, err
	}
	q.mu.Lock()
	defer q.mu.Unlock()
	if q.closed {
		return nil, commonmq.ErrClosed
	}
	options := pulsargo.ProducerOptions{Topic: fullTopic, DisableBatching: !batchingEnabled}
	if q.producerConfig != nil {
		q.producerConfig(&options)
	}
	native, err := q.client.CreateProducer(options)
	if err != nil {
		return nil, fmt.Errorf("create pulsar producer: %w", err)
	}
	return &Producer{producer: native, closeTimeout: q.closeTimeout}, nil
}

func (q *MessageQueue) CreateConsumer(_ context.Context, tenant, namespace, topic, subscription string, _ commonmq.Schema, options commonmq.ConsumerOptions) (commonmq.AbstractConsumer, error) {
	fullTopic, err := commonmq.BuildPulsarTopic(tenant, namespace, topic)
	if err != nil {
		return nil, err
	}
	if subscription == "" {
		return nil, errors.New("pulsar subscription must not be empty")
	}
	q.mu.Lock()
	defer q.mu.Unlock()
	if q.closed {
		return nil, commonmq.ErrClosed
	}
	subscriptionType := pulsargo.Shared
	if value, ok := options.SubscriptionType.(pulsargo.SubscriptionType); ok {
		subscriptionType = value
	}
	nativeOptions := pulsargo.ConsumerOptions{
		Topic:            fullTopic,
		SubscriptionName: subscription,
		Type:             subscriptionType,
	}
	if q.consumerConfig != nil {
		q.consumerConfig(&nativeOptions)
	}
	native, err := q.client.Subscribe(nativeOptions)
	if err != nil {
		return nil, fmt.Errorf("create pulsar consumer: %w", err)
	}
	return &Consumer{
		client:   q.client,
		consumer: native,
		options:  nativeOptions,
		listener: options.MessageListener,
	}, nil
}

func (q *MessageQueue) Close() error {
	q.mu.Lock()
	if q.closed {
		q.mu.Unlock()
		return nil
	}
	q.closed = true
	owns := q.ownsClient
	native := q.client
	q.mu.Unlock()
	if owns && native != nil {
		native.Close()
	}
	return nil
}

type Producer struct {
	mu           sync.Mutex
	closed       bool
	wg           sync.WaitGroup
	producer     producerClient
	closeTimeout time.Duration
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
	_, err = p.producer.Send(ctx, &pulsargo.ProducerMessage{Payload: payload})
	if err != nil {
		return fmt.Errorf("pulsar send: %w", err)
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
	message := &pulsargo.ProducerMessage{Payload: payload}
	p.producer.SendAsync(ctx, message, func(id pulsargo.MessageID, _ *pulsargo.ProducerMessage, sendErr error) {
		if sendErr != nil {
			sendErr = fmt.Errorf("pulsar async send: %w", sendErr)
		}
		messageID := ""
		if id != nil {
			messageID = id.String()
		}
		p.wg.Done()
		if callback != nil {
			callback(commonmq.SendResult{MessageID: messageID, Err: sendErr})
		}
	})
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
	ctx, cancel := context.WithTimeout(context.Background(), p.closeTimeout)
	flushErr := p.producer.FlushWithCtx(ctx)
	cancel()
	p.producer.Close()
	p.wg.Wait()
	return flushErr
}

type Consumer struct {
	opMu     sync.Mutex
	closed   bool
	client   client
	consumer consumerClient
	options  pulsargo.ConsumerOptions
	listener commonmq.MessageListener
}

func (c *Consumer) Receive(ctx context.Context, timeout time.Duration) (*commonmq.Message, error) {
	c.opMu.Lock()
	if c.closed || c.consumer == nil {
		c.opMu.Unlock()
		return nil, commonmq.ErrClosed
	}
	if timeout > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, timeout)
		defer cancel()
	}
	native, err := c.consumer.Receive(ctx)
	if err != nil {
		c.opMu.Unlock()
		if errors.Is(err, context.DeadlineExceeded) {
			return nil, commonmq.ErrTimeout
		}
		return nil, fmt.Errorf("pulsar receive: %w", err)
	}
	message := &commonmq.Message{Payload: append([]byte(nil), native.Payload()...), Native: native}
	listener := c.listener
	c.opMu.Unlock()
	if listener != nil {
		listener(message)
	}
	return message, nil
}

func pulsarMessage(message *commonmq.Message) (pulsargo.Message, error) {
	if message == nil {
		return nil, commonmq.ErrInvalidMessage
	}
	native, ok := message.Native.(pulsargo.Message)
	if !ok || native == nil {
		return nil, commonmq.ErrInvalidMessage
	}
	return native, nil
}

func (c *Consumer) Acknowledge(_ context.Context, message *commonmq.Message) error {
	native, err := pulsarMessage(message)
	if err != nil {
		return err
	}
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed || c.consumer == nil {
		return commonmq.ErrClosed
	}
	if err := c.consumer.Ack(native); err != nil {
		return fmt.Errorf("pulsar acknowledge: %w", err)
	}
	return nil
}

func (c *Consumer) NegativeAcknowledge(_ context.Context, message *commonmq.Message) error {
	native, err := pulsarMessage(message)
	if err != nil {
		return err
	}
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed || c.consumer == nil {
		return commonmq.ErrClosed
	}
	c.consumer.Nack(native)
	return nil
}

func (c *Consumer) Unsubscribe(_ context.Context) error {
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed {
		return commonmq.ErrClosed
	}
	if c.consumer == nil {
		return nil
	}
	if err := c.consumer.Unsubscribe(); err != nil {
		return fmt.Errorf("pulsar unsubscribe: %w", err)
	}
	c.consumer = nil
	return nil
}

func (c *Consumer) Resubscribe(_ context.Context) error {
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed {
		return commonmq.ErrClosed
	}
	if c.consumer != nil {
		c.consumer.Close()
	}
	native, err := c.client.Subscribe(c.options)
	if err != nil {
		c.consumer = nil
		return fmt.Errorf("pulsar resubscribe: %w", err)
	}
	c.consumer = native
	return nil
}

func (c *Consumer) Close() error {
	c.opMu.Lock()
	defer c.opMu.Unlock()
	if c.closed {
		return nil
	}
	c.closed = true
	if c.consumer != nil {
		c.consumer.Close()
		c.consumer = nil
	}
	return nil
}
