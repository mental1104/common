// Package pulsar 实现 Pulsar SDK 与公共 mq Bridge 之间的后端适配层。
// 本包通过窄接口和私有 receipt 隔离 pulsar-client-go 类型。
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

// Config 保存 Pulsar 专属连接、发送和消费参数。
// ServiceURL 必须是 pulsar:// 或 pulsar+ssl:// 地址；AuthenticationToken 可为空。
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

// BackendType 返回该配置对应的 Pulsar 后端类型。
func (Config) BackendType() commonmq.BackendType { return commonmq.BackendPulsar }

// messageIDClient 收窄发送结果所需的 Pulsar MessageID 能力。
type messageIDClient interface {
	String() string
	PartitionIdx() int32
}

// messageClient 收窄消费消息转换和确认所需的 Pulsar Message 能力。
type messageClient interface {
	Topic() string
	Properties() map[string]string
	Payload() []byte
	ID() pulsargo.MessageID
	Key() string
}

// producerClient 收窄 Pulsar Producer 的发送、刷新和关闭能力。
type producerClient interface {
	Send(context.Context, *pulsargo.ProducerMessage) (pulsargo.MessageID, error)
	SendAsync(context.Context, *pulsargo.ProducerMessage, func(pulsargo.MessageID, *pulsargo.ProducerMessage, error))
	FlushWithCtx(context.Context) error
	Close()
}

// consumerClient 收窄 Pulsar Consumer 的接收、确认、订阅和关闭能力。
type consumerClient interface {
	Receive(context.Context) (messageClient, error)
	Ack(messageClient) error
	Nack(messageClient)
	Unsubscribe() error
	Close()
}

// client 收窄 Pulsar Client 创建 Producer/Consumer 和关闭连接的能力。
type client interface {
	CreateProducer(pulsargo.ProducerOptions) (producerClient, error)
	Subscribe(pulsargo.ConsumerOptions) (consumerClient, error)
	Close()
}

// nativeClient 把 pulsargo.Client 适配为本包私有 client 接口。
type nativeClient struct{ pulsargo.Client }

// nativeConsumer 把 pulsargo.Consumer 的 Message 参数转换为私有 messageClient。
type nativeConsumer struct{ pulsargo.Consumer }

// CreateProducer 创建 Pulsar Producer，并把返回值限制为 producerClient。
func (c nativeClient) CreateProducer(options pulsargo.ProducerOptions) (producerClient, error) {
	return c.Client.CreateProducer(options)
}

// Subscribe 创建 Pulsar Consumer，并包裹为 nativeConsumer。
func (c nativeClient) Subscribe(options pulsargo.ConsumerOptions) (consumerClient, error) {
	consumer, err := c.Client.Subscribe(options)
	if err != nil {
		return nil, err
	}
	return nativeConsumer{Consumer: consumer}, nil
}

// Receive 从 Pulsar Consumer 拉取一条消息；ctx 控制取消和超时。
func (c nativeConsumer) Receive(ctx context.Context) (messageClient, error) {
	return c.Consumer.Receive(ctx)
}

// Ack 确认一条来自当前 Pulsar Consumer 的原生消息。
func (c nativeConsumer) Ack(message messageClient) error {
	native, ok := message.(pulsargo.Message)
	if !ok {
		return commonmq.ErrInvalidMessage
	}
	return c.Consumer.Ack(native)
}

// Nack 否认一条来自当前 Pulsar Consumer 的原生消息。
// 类型不匹配时不调用 SDK；上层 receipt 校验会阻止正常路径出现该情况。
func (c nativeConsumer) Nack(message messageClient) {
	if native, ok := message.(pulsargo.Message); ok {
		c.Consumer.Nack(native)
	}
}

// newClient 校验连接地址并创建真实 Pulsar Client。
// 返回的 client 由对应 ProducerBackend 或 ConsumerBackend 独占并负责关闭。
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

// producerBackend 使用一个 Pulsar Client 和 Producer 实现公共 ProducerBackend。
// pending/idle 记录 SDK 已接受请求，closeOnce 保证资源只关闭一次。
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

// NewProducerBackend 创建真实 Pulsar Client 与 Producer，并返回公共 backend 接口。
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

// newProducerBackend 使用已创建的 Client 和 Producer 初始化生命周期状态。
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

// begin 在 backend 未关闭时登记一个发送请求。
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

// finish 完成一个发送，并在全部请求结束时唤醒 Close。
func (b *producerBackend) finish() {
	b.mu.Lock()
	b.pending--
	if b.pending == 0 {
		close(b.idle)
	}
	b.mu.Unlock()
}

// pulsarMessage 把公共 Message 深拷贝为 Pulsar ProducerMessage。
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

// resultFromID 把 Pulsar MessageID 转换为公共 SendResult。
func resultFromID(id messageIDClient, err error) commonmq.SendResult {
	result := commonmq.SendResult{Err: err}
	if id != nil {
		result.MessageID = id.String()
		partition := int(id.PartitionIdx())
		result.Partition = &partition
	}
	return result
}

// Send 同步发送消息，并在 broker 确认后返回 MessageID/Partition。
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

// SendAsync 使用 Pulsar 原生异步发送。
// SDK callback 到达后先释放 pending，再在独立 goroutine 调用用户 callback。
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

// Close 幂等等待已接受请求，刷新 SDK 队列并关闭 Producer 与 Client。
// flush 使用独立 CloseTimeout；调用方 ctx 只控制等待 closeDone 的时限。
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

// consumerBackend 使用 Pulsar Client/Consumer 实现公共 ConsumerBackend。
// receipts 保存尚未确认的 SDK Message，sequence 避免同一 MessageID 重复映射冲突。
type consumerBackend struct {
	client  client
	options pulsargo.ConsumerOptions

	mu       sync.Mutex
	closed   bool
	consumer consumerClient
	receipts map[string]messageClient
	sequence atomic.Uint64
}

// NewConsumerBackend 校验订阅和 topic，创建真实 Pulsar Consumer。
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

// newConsumerBackend 使用已创建的 Client/Consumer 初始化 receipt 存储。
func newConsumerBackend(nativeClient client, nativeConsumer consumerClient, options pulsargo.ConsumerOptions) *consumerBackend {
	return &consumerBackend{
		client:   nativeClient,
		options:  options,
		consumer: nativeConsumer,
		receipts: make(map[string]messageClient),
	}
}

// pulsarSubscriptionType 把公共订阅类型映射为 Pulsar SDK 枚举。
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

// Receive 拉取一条 Pulsar 消息、转换为公共 Message，并保存私有 receipt。
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

// receipt 查找 receipt 对应的 Consumer 和 SDK Message。
// remove 为 true 时在返回前删除凭据，适用于 Pulsar Nack 的 fire-and-forget 语义。
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

// Acknowledge 确认消息；SDK 返回成功后删除 receipt。
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

// NegativeAcknowledge 否认消息并让 broker 按 NackRedeliveryDelay 重投。
func (b *consumerBackend) NegativeAcknowledge(_ context.Context, receiptID string) error {
	consumer, message, err := b.receipt(receiptID, true)
	if err != nil {
		return err
	}
	consumer.Nack(message)
	return nil
}

// Unsubscribe 删除当前 Pulsar 订阅并清空 receipt。
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

// Resubscribe 关闭旧 Consumer，并使用原 options 创建同名订阅 Consumer。
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

// Close 幂等关闭 Consumer 和 Client，并释放 receipt。
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
