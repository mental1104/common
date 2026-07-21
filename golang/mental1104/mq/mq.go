package mq

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
)

var (
	ErrClosed         = errors.New("message queue resource is closed")
	ErrTimeout        = errors.New("message receive timed out")
	ErrInvalidMessage = errors.New("message does not belong to this consumer")
)

type Schema any

type SendResult struct {
	MessageID string
	Err       error
}

type SendCallback func(SendResult)

type Message struct {
	Payload []byte
	Native  any
}

type MessageListener func(*Message)

type ConsumerOptions struct {
	SubscriptionType any
	MessageListener  MessageListener
	Values           map[string]any
}

type AbstractProducer interface {
	Send(context.Context, any) error
	SendAsync(context.Context, any, SendCallback) error
	Close() error
}

type AbstractConsumer interface {
	Receive(context.Context, time.Duration) (*Message, error)
	Acknowledge(context.Context, *Message) error
	NegativeAcknowledge(context.Context, *Message) error
	Unsubscribe(context.Context) error
	Resubscribe(context.Context) error
	Close() error
}

type AbstractMessageQueue interface {
	CreateProducer(context.Context, string, string, string, Schema, bool) (AbstractProducer, error)
	CreateConsumer(context.Context, string, string, string, string, Schema, ConsumerOptions) (AbstractConsumer, error)
	Close() error
}

type Producer = AbstractProducer
type Consumer = AbstractConsumer
type MessageQueue = AbstractMessageQueue

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
			return nil, fmt.Errorf("marshal message record: %w", err)
		}
		return payload, nil
	}
}

func BuildKafkaTopic(tenant, namespace, topic string) (string, error) {
	parts := make([]string, 0, 3)
	for _, part := range []string{tenant, namespace, topic} {
		if strings.TrimSpace(part) != "" {
			parts = append(parts, part)
		}
	}
	if len(parts) == 0 {
		return "", errors.New("kafka topic must not be empty")
	}
	return strings.Join(parts, "."), nil
}

func BuildPulsarTopic(tenant, namespace, topic string) (string, error) {
	if strings.TrimSpace(tenant) == "" || strings.TrimSpace(namespace) == "" || strings.TrimSpace(topic) == "" {
		return "", errors.New("pulsar tenant, namespace and topic must not be empty")
	}
	return fmt.Sprintf("persistent://%s/%s/%s", tenant, namespace, topic), nil
}
