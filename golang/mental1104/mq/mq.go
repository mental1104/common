// Package mq 定义与具体消息队列 SDK 无关的领域模型、错误体系和后端接口。
//
// 上层 Producer、AsyncProducer 与 Consumer 只依赖本包接口；Kafka、Pulsar 等
// SDK 对象必须被限制在各自 backend 包内，不能通过公共 Message 泄漏给调用方。
package mq

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
)

// ErrorCode 表示调用方可以稳定判断的消息队列错误类别。
type ErrorCode string

const (
	// ErrorUnknown 表示暂时无法归入更具体类别的错误。
	ErrorUnknown ErrorCode = "unknown"
	// ErrorInvalidConfig 表示公共配置或后端专属配置不合法。
	ErrorInvalidConfig ErrorCode = "invalid_config"
	// ErrorInvalidMessage 表示消息内容或内部确认凭据无效。
	ErrorInvalidMessage ErrorCode = "invalid_message"
	// ErrorClosed 表示资源已经关闭，不再接受新操作。
	ErrorClosed ErrorCode = "closed"
	// ErrorClosing 表示资源正在关闭，调用方不应继续提交请求。
	ErrorClosing ErrorCode = "closing"
	// ErrorAlreadyStarted 表示 Consumer 已经启动消费循环。
	ErrorAlreadyStarted ErrorCode = "already_started"
	// ErrorTimeout 表示操作超过调用方或后端规定的时限。
	ErrorTimeout ErrorCode = "timeout"
	// ErrorCanceled 表示 context 取消导致操作提前结束。
	ErrorCanceled ErrorCode = "canceled"
	// ErrorBackend 表示具体消息队列后端返回失败。
	ErrorBackend ErrorCode = "backend"
	// ErrorHandler 表示消费 handler 返回错误或发生 panic。
	ErrorHandler ErrorCode = "handler"
)

// MQError 是公共 API 返回的统一错误。
//
// Cause 保存原始错误链，使调用方仍可通过 errors.Is 和 errors.As 判断取消、
// 超时或其他稳定 cause；Backend 只记录后端名称，不暴露具体 SDK 错误类型。
type MQError struct {
	Code      ErrorCode
	Op        string
	Backend   BackendType
	Message   string
	Retryable bool
	Cause     error
}

// Error 返回适合日志记录的人类可读文本。
// nil 接收者返回 "<nil>"，避免错误格式化路径再次 panic。
func (e *MQError) Error() string {
	if e == nil {
		return "<nil>"
	}
	parts := make([]string, 0, 3)
	if e.Op != "" {
		parts = append(parts, e.Op)
	}
	if e.Backend != "" {
		parts = append(parts, string(e.Backend))
	}
	detail := e.Message
	if detail == "" && e.Cause != nil {
		detail = e.Cause.Error()
	}
	if detail == "" {
		detail = string(e.Code)
	}
	parts = append(parts, detail)
	return strings.Join(parts, ": ")
}

// Unwrap 返回底层 cause，供 errors.Is 和 errors.As 遍历错误链。
func (e *MQError) Unwrap() error {
	if e == nil {
		return nil
	}
	return e.Cause
}

// Is 按 ErrorCode 判断两个 MQError 是否属于同一稳定错误类别。
func (e *MQError) Is(target error) bool {
	other, ok := target.(*MQError)
	return ok && other != nil && e != nil && e.Code == other.Code
}

var (
	// ErrClosed 表示资源已关闭。
	ErrClosed = &MQError{Code: ErrorClosed, Message: "message queue resource is closed"}
	// ErrClosing 表示资源正在关闭。
	ErrClosing = &MQError{Code: ErrorClosing, Message: "message queue resource is closing"}
	// ErrAlreadyStarted 表示 Consumer 已存在运行中的消费循环。
	ErrAlreadyStarted = &MQError{Code: ErrorAlreadyStarted, Message: "consumer is already started"}
	// ErrTimeout 表示可重试的超时。
	ErrTimeout = &MQError{Code: ErrorTimeout, Message: "message queue operation timed out", Retryable: true}
	// ErrCanceled 表示 context 取消。
	ErrCanceled = &MQError{Code: ErrorCanceled, Message: "message queue operation canceled", Retryable: true}
	// ErrInvalidMessage 表示 Message 不携带当前 Consumer 生成的确认凭据。
	ErrInvalidMessage = &MQError{Code: ErrorInvalidMessage, Message: "message does not belong to this consumer"}
)

// NewError 构造统一 MQError。
// code 决定稳定错误类别；op 描述失败操作；backend 可为空；message 是面向调用方
// 的补充说明；cause 会被保留在错误链中。返回值始终是新的 *MQError。
func NewError(code ErrorCode, op string, backend BackendType, message string, cause error) error {
	retryable := code == ErrorTimeout || code == ErrorCanceled
	return &MQError{Code: code, Op: op, Backend: backend, Message: message, Cause: cause, Retryable: retryable}
}

// NormalizeError 把任意后端错误转换为 MQError，同时保留已有 MQError 和错误链。
// context deadline/cancel 会优先映射为 ErrorTimeout/ErrorCanceled；err 为 nil 时返回 nil。
func NormalizeError(err error, code ErrorCode, op string, backend BackendType) error {
	if err == nil {
		return nil
	}
	var mqErr *MQError
	if errors.As(err, &mqErr) {
		// 已经完成统一包装时直接返回，避免重复嵌套导致错误码失真。
		return err
	}
	switch {
	case errors.Is(err, context.DeadlineExceeded):
		return NewError(ErrorTimeout, op, backend, "operation timed out", err)
	case errors.Is(err, context.Canceled):
		return NewError(ErrorCanceled, op, backend, "operation canceled", err)
	default:
		return NewError(code, op, backend, "", err)
	}
}

// BackendType 标识实际消息队列后端。
type BackendType string

const (
	// BackendKafka 表示 Kafka 后端。
	BackendKafka BackendType = "kafka"
	// BackendPulsar 表示 Pulsar 后端。
	BackendPulsar BackendType = "pulsar"
)

// BackendConfig 是后端专属配置的最小公共契约。
// 实现只暴露后端类型，具体字段保留在 kafka.Config、pulsar.Config 等类型中。
type BackendConfig interface {
	BackendType() BackendType
}

// Topic 表示跨后端的逻辑主题名称。
// Tenant、Namespace 对 Kafka 可为空并按点号拼接；Pulsar 要求三项都非空。
type Topic struct {
	Tenant    string
	Namespace string
	Name      string
}

// Validate 校验所有后端共同要求的主题字段。
// 当前公共约束只要求 Name 非空；后端附加约束由具体 topic builder 校验。
func (t Topic) Validate() error {
	if strings.TrimSpace(t.Name) == "" {
		return NewError(ErrorInvalidConfig, "validate topic", "", "topic name must not be empty", nil)
	}
	return nil
}

// SubscriptionType 表示跨后端的消费订阅策略。
type SubscriptionType string

const (
	// SubscriptionShared 允许多个 Consumer 共享同一订阅。
	SubscriptionShared SubscriptionType = "shared"
	// SubscriptionExclusive 要求订阅只有一个活动 Consumer。
	SubscriptionExclusive SubscriptionType = "exclusive"
	// SubscriptionFailover 表示主备 Consumer 模式。
	SubscriptionFailover SubscriptionType = "failover"
	// SubscriptionKeyShared 表示同 key 消息固定分派给同一 Consumer。
	SubscriptionKeyShared SubscriptionType = "key_shared"
)

// ProducerConfig 保存 Producer 的公共配置和类型安全的后端配置。
type ProducerConfig struct {
	Topic           Topic
	DisableBatching bool
	Backend         BackendConfig
}

// ConsumerConfig 保存 Consumer 的公共配置和类型安全的后端配置。
// ReceiveTimeout 为零时由调用方 context 或 SDK 默认值控制等待时间。
type ConsumerConfig struct {
	Topic            Topic
	Subscription     string
	SubscriptionType SubscriptionType
	ReceiveTimeout   time.Duration
	Backend          BackendConfig
}

// MessageHeaders 表示随消息传输的字符串键值属性。
type MessageHeaders map[string]string

// Message 是公共层拥有的消息快照。
//
// 发送前调用方可设置 Topic、Key、Payload、Headers 和可选 Partition；ID 与实际
// Partition 通常由后端在发送完成或接收后填充。Key、Payload、Headers 在进入
// Bridge/backend 边界时会复制，调用方可安全复用原切片。receiptID 仅供当前
// Consumer 完成 ack/nack，不能被公共 API 读取或伪造。
type Message struct {
	Topic     string
	Key       []byte
	Payload   []byte
	Headers   MessageHeaders
	Partition *int
	ID        string

	receiptID string
}

// NewMessage 从 payload 创建拥有独立字节副本的 Message。
func NewMessage(payload []byte) Message {
	return Message{Payload: append([]byte(nil), payload...)}
}

// MessageFrom 把常用 Go 值编码成 Message。
// nil 生成空 payload，[]byte 和 string 直接编码，其他值使用 encoding/json。
func MessageFrom(record any) (Message, error) {
	payload, err := MarshalRecord(record)
	if err != nil {
		return Message{}, err
	}
	return NewMessage(payload), nil
}

// CloneMessage 深拷贝调用方可变字段，并保留内部 receiptID。
// 返回值归调用方独立持有，不与输入共享 Key、Payload、Headers 或 Partition 指针。
func CloneMessage(message Message) Message {
	cloned := message
	cloned.Key = append([]byte(nil), message.Key...)
	cloned.Payload = append([]byte(nil), message.Payload...)
	if message.Headers != nil {
		cloned.Headers = make(MessageHeaders, len(message.Headers))
		for key, value := range message.Headers {
			cloned.Headers[key] = value
		}
	}
	if message.Partition != nil {
		partition := *message.Partition
		cloned.Partition = &partition
	}
	return cloned
}

// BackendMessage 是 ConsumerBackend 向 Bridge 返回的消息和私有确认凭据。
// ReceiptID 只能由对应 backend 解释，Bridge 不依赖其编码格式。
type BackendMessage struct {
	Message   Message
	ReceiptID string
}

// NewBackendMessage 创建 BackendMessage，并清除传入 Message 中可能残留的凭据。
func NewBackendMessage(message Message, receiptID string) BackendMessage {
	message.receiptID = ""
	return BackendMessage{Message: CloneMessage(message), ReceiptID: receiptID}
}

// attachReceipt 把 backend 凭据附加到返回给当前 Consumer 的消息副本。
func attachReceipt(message Message, receiptID string) Message {
	cloned := CloneMessage(message)
	cloned.receiptID = receiptID
	return cloned
}

// receiptOf 读取 Message 的内部确认凭据。
// 非当前 Consumer 产生的消息没有凭据，返回 ErrInvalidMessage。
func receiptOf(message Message) (string, error) {
	if message.receiptID == "" {
		return "", ErrInvalidMessage
	}
	return message.receiptID, nil
}

// SendResult 表示一次发送的最终结果。
// MessageID 和 Partition 只在后端能够确定时填写；Err 非 nil 表示发送失败。
type SendResult struct {
	MessageID string
	Partition *int
	Err       error
}

// OK 报告发送结果是否成功。
func (r SendResult) OK() bool { return r.Err == nil }

// DeliveryCallback 在后端接受异步请求后接收唯一一次最终结果。
// callback 由 backend 派发的 goroutine 调用；参数仅在当前调用期间有效。
type DeliveryCallback func(SendResult)

// ConsumeAction 表示 handler 处理消息后的确认决策。
type ConsumeAction int

const (
	// ConsumeLeaveUnacknowledged 不执行 ack 或 nack。
	ConsumeLeaveUnacknowledged ConsumeAction = iota
	// ConsumeAcknowledge 确认消息处理成功。
	ConsumeAcknowledge
	// ConsumeNegativeAcknowledge 否认消息并请求后端重投或重新分配。
	ConsumeNegativeAcknowledge
)

// MessageHandler 在 Consumer 的单一消费 goroutine 中处理消息。
// ctx 在 Stop/Close 时取消；返回 error 或 panic 会被 Bridge 统一转换为 nack。
type MessageHandler func(context.Context, Message) (ConsumeAction, error)

// ProducerBackend 是同步和异步 Producer 共享的后端实现接口。
// 实现必须保证被接受的异步请求最终只调用一次 callback，并使 Close 幂等。
type ProducerBackend interface {
	Send(context.Context, Message) (SendResult, error)
	SendAsync(context.Context, Message, DeliveryCallback) error
	Close(context.Context) error
}

// ConsumerBackend 是 Consumer Bridge 依赖的后端实现接口。
// ReceiptID 的生命周期从 Receive 成功开始，到 ack/nack、取消订阅或 Close 结束。
type ConsumerBackend interface {
	Receive(context.Context) (BackendMessage, error)
	Acknowledge(context.Context, string) error
	NegativeAcknowledge(context.Context, string) error
	Unsubscribe(context.Context) error
	Resubscribe(context.Context) error
	Close(context.Context) error
}

// MarshalRecord 把输入记录编码为独立 payload 字节。
// nil、[]byte、string 使用直接表示，其他值使用 JSON；编码失败返回 ErrorInvalidMessage。
func MarshalRecord(record any) ([]byte, error) {
	switch value := record.(type) {
	case nil:
		return nil, nil
	case []byte:
		return append([]byte(nil), value...), nil
	case string:
		return []byte(value), nil
	default:
		payload, err := json.Marshal(record)
		if err != nil {
			return nil, NewError(ErrorInvalidMessage, "marshal message", "", "", err)
		}
		return payload, nil
	}
}

// BuildKafkaTopic 把非空 tenant、namespace、name 按点号拼接为 Kafka topic。
func BuildKafkaTopic(topic Topic) (string, error) {
	if err := topic.Validate(); err != nil {
		return "", err
	}
	parts := make([]string, 0, 3)
	for _, part := range []string{topic.Tenant, topic.Namespace, topic.Name} {
		if strings.TrimSpace(part) != "" {
			parts = append(parts, part)
		}
	}
	return strings.Join(parts, "."), nil
}

// BuildPulsarTopic 构造 persistent://tenant/namespace/topic 完整主题名。
// Pulsar 三个组成部分都必须非空。
func BuildPulsarTopic(topic Topic) (string, error) {
	if strings.TrimSpace(topic.Tenant) == "" || strings.TrimSpace(topic.Namespace) == "" || strings.TrimSpace(topic.Name) == "" {
		return "", NewError(ErrorInvalidConfig, "build pulsar topic", BackendPulsar, "tenant, namespace and topic must not be empty", nil)
	}
	return fmt.Sprintf("persistent://%s/%s/%s", topic.Tenant, topic.Namespace, topic.Name), nil
}
