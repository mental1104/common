package mq

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
)

type ErrorCode string

const (
	ErrorUnknown        ErrorCode = "unknown"
	ErrorInvalidConfig  ErrorCode = "invalid_config"
	ErrorInvalidMessage ErrorCode = "invalid_message"
	ErrorClosed         ErrorCode = "closed"
	ErrorClosing        ErrorCode = "closing"
	ErrorAlreadyStarted ErrorCode = "already_started"
	ErrorTimeout        ErrorCode = "timeout"
	ErrorCanceled       ErrorCode = "canceled"
	ErrorBackend        ErrorCode = "backend"
	ErrorHandler        ErrorCode = "handler"
)

type MQError struct {
	Code      ErrorCode
	Op        string
	Backend   BackendType
	Message   string
	Retryable bool
	Cause     error
}

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

func (e *MQError) Unwrap() error {
	if e == nil {
		return nil
	}
	return e.Cause
}

func (e *MQError) Is(target error) bool {
	other, ok := target.(*MQError)
	return ok && other != nil && e != nil && e.Code == other.Code
}

var (
	ErrClosed         = &MQError{Code: ErrorClosed, Message: "message queue resource is closed"}
	ErrClosing        = &MQError{Code: ErrorClosing, Message: "message queue resource is closing"}
	ErrAlreadyStarted = &MQError{Code: ErrorAlreadyStarted, Message: "consumer is already started"}
	ErrTimeout        = &MQError{Code: ErrorTimeout, Message: "message queue operation timed out", Retryable: true}
	ErrCanceled       = &MQError{Code: ErrorCanceled, Message: "message queue operation canceled", Retryable: true}
	ErrInvalidMessage = &MQError{Code: ErrorInvalidMessage, Message: "message does not belong to this consumer"}
)

func NewError(code ErrorCode, op string, backend BackendType, message string, cause error) error {
	retryable := code == ErrorTimeout || code == ErrorCanceled
	return &MQError{Code: code, Op: op, Backend: backend, Message: message, Cause: cause, Retryable: retryable}
}

func NormalizeError(err error, code ErrorCode, op string, backend BackendType) error {
	if err == nil {
		return nil
	}
	var mqErr *MQError
	if errors.As(err, &mqErr) {
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

type BackendType string

const (
	BackendKafka  BackendType = "kafka"
	BackendPulsar BackendType = "pulsar"
)

type BackendConfig interface {
	BackendType() BackendType
}

type Topic struct {
	Tenant    string
	Namespace string
	Name      string
}

func (t Topic) Validate() error {
	if strings.TrimSpace(t.Name) == "" {
		return NewError(ErrorInvalidConfig, "validate topic", "", "topic name must not be empty", nil)
	}
	return nil
}

type SubscriptionType string

const (
	SubscriptionShared    SubscriptionType = "shared"
	SubscriptionExclusive SubscriptionType = "exclusive"
	SubscriptionFailover  SubscriptionType = "failover"
	SubscriptionKeyShared SubscriptionType = "key_shared"
)

type ProducerConfig struct {
	Topic           Topic
	DisableBatching bool
	Backend         BackendConfig
}

type ConsumerConfig struct {
	Topic            Topic
	Subscription     string
	SubscriptionType SubscriptionType
	ReceiveTimeout   time.Duration
	Backend          BackendConfig
}

type MessageHeaders map[string]string

type Message struct {
	Topic     string
	Key       []byte
	Payload   []byte
	Headers   MessageHeaders
	Partition *int
	ID        string

	receiptID string
}

func NewMessage(payload []byte) Message {
	return Message{Payload: append([]byte(nil), payload...)}
}

func MessageFrom(record any) (Message, error) {
	payload, err := MarshalRecord(record)
	if err != nil {
		return Message{}, err
	}
	return NewMessage(payload), nil
}

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

type BackendMessage struct {
	Message   Message
	ReceiptID string
}

func NewBackendMessage(message Message, receiptID string) BackendMessage {
	message.receiptID = ""
	return BackendMessage{Message: CloneMessage(message), ReceiptID: receiptID}
}

func attachReceipt(message Message, receiptID string) Message {
	cloned := CloneMessage(message)
	cloned.receiptID = receiptID
	return cloned
}

func receiptOf(message Message) (string, error) {
	if message.receiptID == "" {
		return "", ErrInvalidMessage
	}
	return message.receiptID, nil
}

type SendResult struct {
	MessageID string
	Partition *int
	Err       error
}

func (r SendResult) OK() bool { return r.Err == nil }

type DeliveryCallback func(SendResult)

type ConsumeAction int

const (
	ConsumeLeaveUnacknowledged ConsumeAction = iota
	ConsumeAcknowledge
	ConsumeNegativeAcknowledge
)

type MessageHandler func(context.Context, Message) (ConsumeAction, error)

type ProducerBackend interface {
	Send(context.Context, Message) (SendResult, error)
	SendAsync(context.Context, Message, DeliveryCallback) error
	Close(context.Context) error
}

type ConsumerBackend interface {
	Receive(context.Context) (BackendMessage, error)
	Acknowledge(context.Context, string) error
	NegativeAcknowledge(context.Context, string) error
	Unsubscribe(context.Context) error
	Resubscribe(context.Context) error
	Close(context.Context) error
}

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

func BuildPulsarTopic(topic Topic) (string, error) {
	if strings.TrimSpace(topic.Tenant) == "" || strings.TrimSpace(topic.Namespace) == "" || strings.TrimSpace(topic.Name) == "" {
		return "", NewError(ErrorInvalidConfig, "build pulsar topic", BackendPulsar, "tenant, namespace and topic must not be empty", nil)
	}
	return fmt.Sprintf("persistent://%s/%s/%s", topic.Tenant, topic.Namespace, topic.Name), nil
}
