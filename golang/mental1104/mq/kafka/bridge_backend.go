// Package kafka 实现 Kafka SDK 与公共 mq Bridge 之间的后端适配层。
// 本包不向业务调用方暴露 kafka-go 的 Message、Reader 或 Writer 类型。
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

// AckMode 表示 Kafka producer 等待 broker 确认的级别。
type AckMode int

const (
	// AckAll 等待所有同步副本确认，提供最强持久性保证。
	AckAll AckMode = iota
	// AckLeader 只等待分区 leader 确认。
	AckLeader
	// AckNone 不等待 broker 确认，调用方无法获知持久化结果。
	AckNone
)

// Config 保存 Kafka 专属连接、批处理和消费参数。
// Brokers 至少包含一个 bootstrap 地址；AllowAutoTopicCreation 仅建议用于测试环境。
type Config struct {
	Brokers                []string
	DialTimeout            time.Duration
	ReadTimeout            time.Duration
	WriteTimeout           time.Duration
	BatchSize              int
	BatchBytes             int64
	BatchTimeout           time.Duration
	QueueCapacity          int
	MinBytes               int
	MaxBytes               int
	MaxWait                time.Duration
	AckMode                AckMode
	AllowAutoTopicCreation bool
}

// BackendType 返回该配置对应的 Kafka 后端类型。
func (Config) BackendType() commonmq.BackendType { return commonmq.BackendKafka }

// writer 收窄 kafka-go Writer，便于后端只依赖实际使用的 SDK 能力。
type writer interface {
	WriteMessages(context.Context, ...kafkago.Message) error
	Close() error
}

// reader 收窄 kafka-go Reader，隔离拉取、提交和关闭能力。
type reader interface {
	FetchMessage(context.Context) (kafkago.Message, error)
	CommitMessages(context.Context, ...kafkago.Message) error
	Close() error
}

// readerFactory 创建同一 topic 和 consumer group 的新 Reader。
// Kafka nack/resubscribe 需要重建 Reader，因此工厂必须可重复调用。
type readerFactory func() (reader, error)

// operationTracker 记录 ProducerBackend 已接受但尚未完成的发送。
// mu 保护 closed、pending 和 idle；idle 只在 pending 从 0 变为 1 时重建。
type operationTracker struct {
	mu      sync.Mutex
	closed  bool
	pending int
	idle    chan struct{}
}

// newOperationTracker 创建初始空闲的操作计数器。
func newOperationTracker() operationTracker {
	idle := make(chan struct{})
	close(idle)
	return operationTracker{idle: idle}
}

// begin 在 backend 未关闭时登记一个发送操作。
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

// finish 完成一个发送，并在全部操作结束时唤醒 Close。
func (t *operationTracker) finish() {
	t.mu.Lock()
	t.pending--
	if t.pending == 0 {
		close(t.idle)
	}
	t.mu.Unlock()
}

// closeAndIdle 原子拒绝新操作，并返回当前批次的空闲信号。
func (t *operationTracker) closeAndIdle() <-chan struct{} {
	t.mu.Lock()
	t.closed = true
	idle := t.idle
	t.mu.Unlock()
	return idle
}

// requiredAcks 把公共 AckMode 映射为 kafka-go RequiredAcks。
func requiredAcks(mode AckMode) kafkago.RequiredAcks {
	switch mode {
	case AckNone:
		return kafkago.RequireNone
	case AckLeader:
		return kafkago.RequireOne
	case AckAll:
		fallthrough
	default:
		return kafkago.RequireAll
	}
}

// newWriter 创建同步 kafka-go Writer。
// Async 保持 false，因为异步语义由 Bridge/backend goroutine 管理，必须保留最终错误。
func newWriter(config Config, topic string, disableBatching bool) writer {
	result := &kafkago.Writer{
		Addr:                   kafkago.TCP(config.Brokers...),
		Topic:                  topic,
		RequiredAcks:           requiredAcks(config.AckMode),
		ReadTimeout:            config.ReadTimeout,
		WriteTimeout:           config.WriteTimeout,
		BatchSize:              config.BatchSize,
		BatchBytes:             config.BatchBytes,
		BatchTimeout:           config.BatchTimeout,
		Async:                  false,
		AllowAutoTopicCreation: config.AllowAutoTopicCreation,
	}
	if disableBatching {
		result.BatchSize = 1
		result.BatchTimeout = 0
	}
	return result
}

// newReader 返回可重复创建同一 consumer group Reader 的工厂。
// CommitInterval 为零，确保 ack 通过 CommitMessages 同步提交。
func newReader(config Config, topic, subscription string) readerFactory {
	return func() (reader, error) {
		readerConfig := kafkago.ReaderConfig{
			Brokers:        append([]string(nil), config.Brokers...),
			GroupID:        subscription,
			Topic:          topic,
			QueueCapacity:  config.QueueCapacity,
			MinBytes:       config.MinBytes,
			MaxBytes:       config.MaxBytes,
			MaxWait:        config.MaxWait,
			CommitInterval: 0,
		}
		if config.DialTimeout > 0 {
			readerConfig.Dialer = &kafkago.Dialer{Timeout: config.DialTimeout}
		}
		return kafkago.NewReader(readerConfig), nil
	}
}

// producerBackend 使用一个 kafka-go Writer 实现公共 ProducerBackend。
// tracker 与 closeOnce 保证 Close 等待所有已接受发送，并且底层 Writer 只关闭一次。
type producerBackend struct {
	writer    writer
	tracker   operationTracker
	closeOnce sync.Once
	closeDone chan struct{}
	closeErr  error
}

// NewProducerBackend 校验 Kafka 配置、构造完整 topic，并返回 SDK 隔离后的 backend。
func NewProducerBackend(config commonmq.ProducerConfig, backend Config) (commonmq.ProducerBackend, error) {
	if len(backend.Brokers) == 0 {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create kafka producer backend", commonmq.BackendKafka, "brokers must not be empty", nil)
	}
	topic, err := commonmq.BuildKafkaTopic(config.Topic)
	if err != nil {
		return nil, err
	}
	return newProducerBackend(newWriter(backend, topic, config.DisableBatching)), nil
}

// newProducerBackend 使用已经创建的 Writer 初始化 backend。
func newProducerBackend(native writer) *producerBackend {
	return &producerBackend{
		writer:    native,
		tracker:   newOperationTracker(),
		closeDone: make(chan struct{}),
	}
}

// kafkaMessage 把公共 Message 深拷贝为 kafka-go Message。
func kafkaMessage(message commonmq.Message) kafkago.Message {
	headers := make([]kafkago.Header, 0, len(message.Headers))
	for key, value := range message.Headers {
		headers = append(headers, kafkago.Header{Key: key, Value: []byte(value)})
	}
	native := kafkago.Message{
		Key:     append([]byte(nil), message.Key...),
		Value:   append([]byte(nil), message.Payload...),
		Headers: headers,
	}
	if message.Partition != nil {
		native.Partition = *message.Partition
	}
	return native
}

// Send 同步写入 Kafka，并把 SDK 错误转换为 MQError。
func (b *producerBackend) Send(ctx context.Context, message commonmq.Message) (commonmq.SendResult, error) {
	if err := b.tracker.begin(); err != nil {
		return commonmq.SendResult{Err: err}, err
	}
	defer b.tracker.finish()
	if err := b.writer.WriteMessages(ctx, kafkaMessage(message)); err != nil {
		err = commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka send", commonmq.BackendKafka)
		return commonmq.SendResult{Err: err}, err
	}
	result := commonmq.SendResult{}
	if message.Partition != nil {
		partition := *message.Partition
		result.Partition = &partition
	}
	return result, nil
}

// SendAsync 在独立 goroutine 中执行可观测结果的同步 WriteMessages。
// 请求被登记后最终只调用一次 callback；finish 在 callback 前执行，允许 callback 关闭 Producer。
func (b *producerBackend) SendAsync(ctx context.Context, message commonmq.Message, callback commonmq.DeliveryCallback) error {
	if err := b.tracker.begin(); err != nil {
		return err
	}
	native := kafkaMessage(message)
	go func() {
		err := b.writer.WriteMessages(ctx, native)
		if err != nil {
			err = commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka async send", commonmq.BackendKafka)
		}
		b.tracker.finish()
		if callback != nil {
			partition := message.Partition
			result := commonmq.SendResult{Partition: partition, Err: err}
			// 用户 callback 与 SDK 写入 goroutine 分离，panic 不会破坏 backend 状态。
			go func() {
				defer func() { _ = recover() }()
				callback(result)
			}()
		}
	}()
	return nil
}

// Close 幂等等待全部发送结束后关闭 Writer。
// ctx 取消只终止当前等待，不会撤销已经启动的底层关闭 goroutine。
func (b *producerBackend) Close(ctx context.Context) error {
	b.closeOnce.Do(func() {
		go func() {
			idle := b.tracker.closeAndIdle()
			<-idle
			b.closeErr = commonmq.NormalizeError(b.writer.Close(), commonmq.ErrorBackend, "close kafka producer", commonmq.BackendKafka)
			close(b.closeDone)
		}()
	})
	select {
	case <-b.closeDone:
		return b.closeErr
	case <-ctx.Done():
		return commonmq.NormalizeError(ctx.Err(), commonmq.ErrorCanceled, "close kafka producer", commonmq.BackendKafka)
	}
}

// consumerBackend 使用 kafka-go Reader 实现公共 ConsumerBackend。
// receipts 保存未确认 SDK Message，只有对应 receiptID 才能提交或否认。
type consumerBackend struct {
	topic         string
	subscription  string
	readerFactory readerFactory

	mu       sync.Mutex
	closed   bool
	reader   reader
	receipts map[string]kafkago.Message
	sequence atomic.Uint64
}

// NewConsumerBackend 校验配置并创建初始 Kafka Reader。
func NewConsumerBackend(config commonmq.ConsumerConfig, backend Config) (commonmq.ConsumerBackend, error) {
	if len(backend.Brokers) == 0 {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create kafka consumer backend", commonmq.BackendKafka, "brokers must not be empty", nil)
	}
	if config.Subscription == "" {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create kafka consumer backend", commonmq.BackendKafka, "subscription must not be empty", nil)
	}
	topic, err := commonmq.BuildKafkaTopic(config.Topic)
	if err != nil {
		return nil, err
	}
	factory := newReader(backend, topic, config.Subscription)
	native, err := factory()
	if err != nil {
		return nil, commonmq.NormalizeError(err, commonmq.ErrorBackend, "create kafka reader", commonmq.BackendKafka)
	}
	return newConsumerBackend(topic, config.Subscription, factory, native), nil
}

// newConsumerBackend 使用已创建的 Reader 初始化 receipt 存储。
func newConsumerBackend(topic, subscription string, factory readerFactory, native reader) *consumerBackend {
	return &consumerBackend{
		topic:         topic,
		subscription:  subscription,
		readerFactory: factory,
		reader:        native,
		receipts:      make(map[string]kafkago.Message),
	}
}

// Receive 拉取一条 Kafka 消息并转换为公共 Message。
// 返回前保存 SDK Message；调用方必须使用对应 receiptID 完成 ack 或 nack。
func (b *consumerBackend) Receive(ctx context.Context) (commonmq.BackendMessage, error) {
	b.mu.Lock()
	if b.closed || b.reader == nil {
		b.mu.Unlock()
		return commonmq.BackendMessage{}, commonmq.ErrClosed
	}
	nativeReader := b.reader
	b.mu.Unlock()

	message, err := nativeReader.FetchMessage(ctx)
	if err != nil {
		return commonmq.BackendMessage{}, err
	}
	receiptID := fmt.Sprintf("%s:%d:%d:%d", message.Topic, message.Partition, message.Offset, b.sequence.Add(1))
	headers := make(commonmq.MessageHeaders, len(message.Headers))
	for _, header := range message.Headers {
		headers[header.Key] = string(header.Value)
	}
	partition := message.Partition
	domain := commonmq.Message{
		Topic:     message.Topic,
		Key:       append([]byte(nil), message.Key...),
		Payload:   append([]byte(nil), message.Value...),
		Headers:   headers,
		Partition: &partition,
		ID:        message.Topic + "/" + strconv.Itoa(message.Partition) + "/" + strconv.FormatInt(message.Offset, 10),
	}
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return commonmq.BackendMessage{}, commonmq.ErrClosed
	}
	b.receipts[receiptID] = message
	b.mu.Unlock()
	return commonmq.NewBackendMessage(domain, receiptID), nil
}

// takeReceipt 查找 receipt 对应的 Reader 和 SDK Message。
// remove 为 true 时在返回前移除凭据，适用于不需要等待 broker 结果的 nack。
func (b *consumerBackend) takeReceipt(receiptID string, remove bool) (reader, kafkago.Message, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.closed || b.reader == nil {
		return nil, kafkago.Message{}, commonmq.ErrClosed
	}
	message, ok := b.receipts[receiptID]
	if !ok {
		return nil, kafkago.Message{}, commonmq.ErrInvalidMessage
	}
	if remove {
		delete(b.receipts, receiptID)
	}
	return b.reader, message, nil
}

// Acknowledge 同步提交消息 offset；提交成功后删除 receipt。
func (b *consumerBackend) Acknowledge(ctx context.Context, receiptID string) error {
	nativeReader, message, err := b.takeReceipt(receiptID, false)
	if err != nil {
		return err
	}
	if err := nativeReader.CommitMessages(ctx, message); err != nil {
		return commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka acknowledge", commonmq.BackendKafka)
	}
	b.mu.Lock()
	delete(b.receipts, receiptID)
	b.mu.Unlock()
	return nil
}

// NegativeAcknowledge 关闭当前 Reader 并以相同 group 配置重建。
// kafka-go 没有与 Pulsar 单消息 nack 完全等价的 API；未提交 offset 将由 group 重新分配。
func (b *consumerBackend) NegativeAcknowledge(_ context.Context, receiptID string) error {
	b.mu.Lock()
	if b.closed || b.reader == nil {
		b.mu.Unlock()
		return commonmq.ErrClosed
	}
	if _, ok := b.receipts[receiptID]; !ok {
		b.mu.Unlock()
		return commonmq.ErrInvalidMessage
	}
	current := b.reader
	b.mu.Unlock()

	if err := current.Close(); err != nil {
		return commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka negative acknowledge close", commonmq.BackendKafka)
	}
	next, err := b.readerFactory()
	if err != nil {
		b.mu.Lock()
		b.reader = nil
		b.mu.Unlock()
		return commonmq.NormalizeError(err, commonmq.ErrorBackend, "kafka negative acknowledge resubscribe", commonmq.BackendKafka)
	}
	b.mu.Lock()
	b.reader = next
	b.receipts = make(map[string]kafkago.Message)
	b.mu.Unlock()
	return nil
}

// Unsubscribe 关闭当前 Reader 并清空未确认 receipt。
// Kafka consumer group 元数据由 broker 按其保留策略管理，本方法不删除 group。
func (b *consumerBackend) Unsubscribe(context.Context) error {
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return commonmq.ErrClosed
	}
	current := b.reader
	b.reader = nil
	b.receipts = make(map[string]kafkago.Message)
	b.mu.Unlock()
	if current == nil {
		return nil
	}
	return commonmq.NormalizeError(current.Close(), commonmq.ErrorBackend, "unsubscribe kafka consumer", commonmq.BackendKafka)
}

// Resubscribe 关闭旧 Reader，并使用相同 topic/group 创建新 Reader。
func (b *consumerBackend) Resubscribe(context.Context) error {
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return commonmq.ErrClosed
	}
	current := b.reader
	b.mu.Unlock()
	if current != nil {
		if err := current.Close(); err != nil {
			return commonmq.NormalizeError(err, commonmq.ErrorBackend, "close kafka reader before resubscribe", commonmq.BackendKafka)
		}
	}
	next, err := b.readerFactory()
	if err != nil {
		return commonmq.NormalizeError(err, commonmq.ErrorBackend, "resubscribe kafka consumer", commonmq.BackendKafka)
	}
	b.mu.Lock()
	b.reader = next
	b.receipts = make(map[string]kafkago.Message)
	b.mu.Unlock()
	return nil
}

// Close 幂等关闭 Reader 并释放所有 receipt。
func (b *consumerBackend) Close(context.Context) error {
	b.mu.Lock()
	if b.closed {
		b.mu.Unlock()
		return nil
	}
	b.closed = true
	current := b.reader
	b.reader = nil
	b.receipts = nil
	b.mu.Unlock()
	if current == nil {
		return nil
	}
	return commonmq.NormalizeError(current.Close(), commonmq.ErrorBackend, "close kafka consumer", commonmq.BackendKafka)
}

var _ commonmq.ProducerBackend = (*producerBackend)(nil)
var _ commonmq.ConsumerBackend = (*consumerBackend)(nil)
