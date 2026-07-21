package pulsar

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	pulsargo "github.com/apache/pulsar-client-go/pulsar"
	commonmq "github.com/mental1104/common/golang/mental1104/mq"
)

type Config struct {
	ServiceURL          string
	AuthenticationToken string
	ConnectionTimeout   time.Duration
	OperationTimeout    time.Duration
	CloseTimeout        time.Duration
	SendTimeout         time.Duration
	MaxPendingMessages  int
	ReceiverQueueSize   int
	NackRedeliveryDelay time.Duration
}

func (Config) BackendType() commonmq.BackendType { return commonmq.BackendPulsar }

type messageIDClient interface {
	String() string
	PartitionIdx() int32
}

type messageClient interface {
	Topic() string
	Properties() map[string]string
	Payload() []byte
	ID() pulsargo.MessageID
	Key() string
}

type producerClient interface {
	Send(context.Context, *pulsargo.ProducerMessage) (pulsargo.MessageID, error)
	SendAsync(context.Context, *pulsargo.ProducerMessage, func(pulsargo.MessageID, *pulsargo.ProducerMessage, error))
	FlushWithCtx(context.Context) error
	Close()
}

type consumerClient interface {
	Receive(context.Context) (messageClient, error)
	Ack(messageClient) error
	Nack(messageClient)
	Unsubscribe() error
	Close()
}

type client interface {
	CreateProducer(pulsargo.ProducerOptions) (producerClient, error)
	Subscribe(pulsargo.ConsumerOptions) (consumerClient, error)
	Close()
}

type nativeClient struct{ pulsargo.Client }

type nativeConsumer struct{ pulsargo.Consumer }

func (c nativeClient) CreateProducer(options pulsargo.ProducerOptions) (producerClient, error) {
	return c.Client.CreateProducer(options)
}

func (c nativeClient) Subscribe(options pulsargo.ConsumerOptions) (consumerClient, error) {
	consumer, err := c.Client.Subscribe(options)
	if err != nil {
		return nil, err
	}
	return nativeConsumer{Consumer: consumer}, nil
}

func (c nativeConsumer) Receive(ctx context.Context) (messageClient, error) {
	return c.Consumer.Receive(ctx)
}

func (c nativeConsumer) Ack(message messageClient) error {
	native, ok := message.(pulsargo.Message)
	if !ok {
		return commonmq.ErrInvalidMessage
	}
	return c.Consumer.Ack(native)
}

func (c nativeConsumer) Nack(message messageClient) {
	if native, ok := message.(pulsargo.Message); ok {
		c.Consumer.Nack(native)
	}
}

func newClient(config Config) (client, error) {
	if config.ServiceURL == "" {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create pulsar client", commonmq.BackendPulsar, "service URL must not be empty", nil)
	}
	options := pulsargo.ClientOptions{
		URL:               config.ServiceURL,
		ConnectionTimeout: config.ConnectionTimeout,
		OperationTimeout:  config.OperationTimeout,
	}
	if config.AuthenticationToken != "" {
		options.Authentication = pulsargo.NewAuthenticationToken(config.AuthenticationToken)
	}
	native, err := pulsargo.NewClient(options)
	if err != nil {
		return nil, commonmq.NormalizeError(err, commonmq.ErrorBackend, "create pulsar client", commonmq.BackendPulsar)
	}
	return nativeClient{Client: native}, nil
}

type producerBackend struct {
	client     client
	producer   producerClient
	closeDelay time.Duration

	mu        sync.Mutex
	closed    bool
	pending   int
	idle      chan struct{}
	closeOnce sync.Once
	closeDone chan struct{}
	closeErr  error
}

func NewProducerBackend(config commonmq.ProducerConfig, backend Config) (commonmq.ProducerBackend, error) {
	topic, err := commonmq.BuildPulsarTopic(config.Topic)
	if err != nil {
		return nil, err
	}
	nativeClient, err := newClient(backend)
	if err != nil {
		return nil, err
	}
	options := pulsargo.ProducerOptions{
		Topic:                   topic,
		DisableBatching:         config.DisableBatching,
		SendTimeout:             backend.SendTimeout,
		MaxPendingMessages:      backend.MaxPendingMessages,
		DisableBlockIfQueueFull: false,
	}
	nativeProducer, err := nativeClient.CreateProducer(options)
	if err != nil {
		nativeClient.Close()
		return nil, commonmq.NormalizeError(err, commonmq.ErrorBackend, "create pulsar producer", commonmq.BackendPulsar)
	}
	return newProducerBackend(nativeClient, nativeProducer, backend.CloseTimeout), nil
}

func newProducerBackend(nativeClient client, nativeProducer producerClient, closeTimeout time.Duration) *producerBackend {
	idle := make(chan struct{})
	close(idle)
	if closeTimeout <= 0 {
		closeTimeout = 10 * time.Second
	}
	return &producerBackend{
		client:     nativeClient,
		producer:   nativeProducer,
		closeDelay: closeTimeout,
		idle:       idle,
		closeDone:  make(chan struct{}),
	}
}

func (b *producerBackend) begin() error {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed {
		return commonmq.ErrClosed
	}
	if b.pending == 0 {
		b.idle = make(chan struct{})
	}
	b.pending++
	return nil
}

func (b *producerBackend) finish() {
	b.mu.Lock()
	b.pending--
	if b.pending == 0 {
		close(b.idle)
	}
	b.mu.Unlock()
}

func pulsarMessage(message commonmq.Message) *pulsargo.ProducerMessage {
	properties := make(map[string]string, len(message.Headers))
	for key, value := range message.Headers {
		properties[key] = value
	}
	return &pulsargo.ProducerMessage{
		Payload:    append([]byte(nil), message.Payload...),
		Key:        string(message.Key),
		Properties: properties,
	}
}

func resultFromID(id messageIDClient, err error) commonmq.SendResult {
	result := commonmq.SendResult{Err: err}
	if id != nil {
		result.MessageID = id.String()
		partition := int(id.PartitionIdx())
		result.Partition = &partition
	}
	return result
}

func (b *producerBackend) Send(ctx context.Context, message commonmq.Message) (commonmq.SendResult, error) {
	if err := b.begin(); err != nil {
		return commonmq.SendResult{Err: err}, err
	}
	defer b.finish()
	id, err := b.producer.Send(ctx, pulsarMessage(message))
	if err != nil {
		err = commonmq.NormalizeError(err, commonmq.ErrorBackend, "pulsar send", commonmq.BackendPulsar)
		return resultFromID(id, err), err
	}
	return resultFromID(id, nil), nil
}

func (b *producerBackend) SendAsync(ctx context.Context, message commonmq.Message, callback commonmq.DeliveryCallback) error {
	if err := b.begin(); err != nil {
		return err
	}
	b.producer.SendAsync(ctx, pulsarMessage(message), func(id pulsargo.MessageID, _ *pulsargo.ProducerMessage, err error) {
		if err != nil {
			err = commonmq.NormalizeError(err, commonmq.ErrorBackend, "pulsar async send", commonmq.BackendPulsar)
		}
		b.finish()
		if callback != nil {
			result := resultFromID(id, err)
			go func() {
				defer func() { _ = recover() }()
				callback(result)
			}()
		}
	})
	return nil
}

func (b *producerBackend) Close(ctx context.Context) error {
	b.closeOnce.Do(func() {
		b.mu.Lock()
		b.closed = true
		idle := b.idle
		b.mu.Unlock()
		go func() {
			<-idle
			flushCtx, cancel := context.WithTimeout(context.Background(), b.closeDelay)
			flushErr := b.producer.FlushWithCtx(flushCtx)
			cancel()
			b.producer.Close()
			b.client.Close()
			b.closeErr = commonmq.NormalizeError(flushErr, commonmq.ErrorBackend, "close pulsar producer", commonmq.BackendPulsar)
			close(b.closeDone)
		}()
	})
	select {
	case <-b.closeDone:
		return b.closeErr
	case <-ctx.Done():
		return commonmq.NormalizeError(ctx.Err(), commonmq.ErrorCanceled, "close pulsar producer", commonmq.BackendPulsar)
	}
}

type consumerBackend struct {
	client  client
	options pulsargo.ConsumerOptions

	mu       sync.Mutex
	closed   bool
	consumer consumerClient
	receipts map[string]messageClient
	sequence atomic.Uint64
}

func NewConsumerBackend(config commonmq.ConsumerConfig, backend Config) (commonmq.ConsumerBackend, error) {
	if config.Subscription == "" {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create pulsar consumer", commonmq.BackendPulsar, "subscription must not be empty", nil)
	}
	topic, err := commonmq.BuildPulsarTopic(config.Topic)
	if err != nil {
		return nil, err
	}
	nativeClient, err := newClient(backend)
	if err != nil {
		return nil, err
	}
	options := pulsargo.ConsumerOptions{
		Topic:               topic,
		SubscriptionName:    config.Subscription,
		Type:                pulsarSubscriptionType(config.SubscriptionType),
		ReceiverQueueSize:   backend.ReceiverQueueSize,
		NackRedeliveryDelay: backend.NackRedeliveryDelay,
	}
	nativeConsumer, err := nativeClient.Subscribe(options)
	if err != nil {
		nativeClient.Close()
		return nil, commonmq.NormalizeError(err, commonmq.ErrorBackend, "create pulsar consumer", commonmq.BackendPulsar)
	}
	return newConsumerBackend(nativeClient, nativeConsumer, options), nil
}

func newConsumerBackend(nativeClient client, nativeConsumer consumerClient, options pulsargo.ConsumerOptions) *consumerBackend {
	return &consumerBackend{
		client:   nativeClient,
		options:  options,
		consumer: nativeConsumer,
		receipts: make(map[string]messageClient),
	}
}

func pulsarSubscriptionType(value commonmq.SubscriptionType) pulsargo.SubscriptionType {
	switch value {
	case commonmq.SubscriptionExclusive:
		return pulsargo.Exclusive
	case commonmq.SubscriptionFailover:
		return pulsargo.Failover
	case commonmq.SubscriptionKeyShared:
		return pulsargo.KeyShared
	case commonmq.SubscriptionShared:
		fallthrough
	default:
		return pulsargo.Shared
	}
}

func (b *consumerBackend) Receive(ctx context.Context) (commonmq.BackendMessage, error) {
	b.mu.Lock()
	if b.closed || b.consumer == nil {
		b.mu.Unlock()
		return commonmq.BackendMessage{}, commonmq.ErrClosed
	}
	nativeConsumer := b.consumer
	b.mu.Unlock()

	native, err := nativeConsumer.Receive(ctx)
	if err != nil {
		return commonmq.BackendMessage{}, err
	}
	id := native.ID()
	receiptID := fmt.Sprintf("%s:%d", id.String(), b.sequence.Add(1))
	headers := make(commonmq.MessageHeaders, len(native.Properties()))
	for key, value := range native.Properties() {
		headers[key] = value
	}
	partition := int(id.PartitionIdx())
	message := commonmq.Message{
		Topic:     native.Topic(),
		Key:       []byte(native.Key()),
		Payload:   append([]byte(nil), native.Payload()...),
		Headers:   headers,
		Partition: &partition,
		ID:        id.String(),
	}
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return commonmq.BackendMessage{}, commonmq.ErrClosed
	}
	b.receipts[receiptID] = native
	b.mu.Unlock()
	return commonmq.NewBackendMessage(message, receiptID), nil
}

func (b *consumerBackend) receipt(receiptID string, remove bool) (consumerClient, messageClient, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed || b.consumer == nil {
		return nil, nil, commonmq.ErrClosed
	}
	message, ok := b.receipts[receiptID]
	if !ok {
		return nil, nil, commonmq.ErrInvalidMessage
	}
	if remove {
		delete(b.receipts, receiptID)
	}
	return b.consumer, message, nil
}

func (b *consumerBackend) Acknowledge(_ context.Context, receiptID string) error {
	consumer, message, err := b.receipt(receiptID, false)
	if err != nil {
		return err
	}
	if err := consumer.Ack(message); err != nil {
		return commonmq.NormalizeError(err, commonmq.ErrorBackend, "pulsar acknowledge", commonmq.BackendPulsar)
	}
	b.mu.Lock()
	delete(b.receipts, receiptID)
	b.mu.Unlock()
	return nil
}

func (b *consumerBackend) NegativeAcknowledge(_ context.Context, receiptID string) error {
	consumer, message, err := b.receipt(receiptID, true)
	if err != nil {
		return err
	}
	consumer.Nack(message)
	return nil
}

func (b *consumerBackend) Unsubscribe(context.Context) error {
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return commonmq.ErrClosed
	}
	current := b.consumer
	b.consumer = nil
	b.receipts = make(map[string]messageClient)
	b.mu.Unlock()
	if current == nil {
		return nil
	}
	return commonmq.NormalizeError(current.Unsubscribe(), commonmq.ErrorBackend, "unsubscribe pulsar consumer", commonmq.BackendPulsar)
}

func (b *consumerBackend) Resubscribe(context.Context) error {
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return commonmq.ErrClosed
	}
	current := b.consumer
	b.mu.Unlock()
	if current != nil {
		current.Close()
	}
	next, err := b.client.Subscribe(b.options)
	if err != nil {
		return commonmq.NormalizeError(err, commonmq.ErrorBackend, "resubscribe pulsar consumer", commonmq.BackendPulsar)
	}
	b.mu.Lock()
	b.consumer = next
	b.receipts = make(map[string]messageClient)
	b.mu.Unlock()
	return nil
}

func (b *consumerBackend) Close(context.Context) error {
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return nil
	}
	b.closed = true
	current := b.consumer
	b.consumer = nil
	b.receipts = nil
	b.mu.Unlock()
	if current != nil {
		current.Close()
	}
	b.client.Close()
	return nil
}

var _ commonmq.ProducerBackend = (*producerBackend)(nil)
var _ commonmq.ConsumerBackend = (*consumerBackend)(nil)
