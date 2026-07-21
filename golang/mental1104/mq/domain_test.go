package mq

import (
	"errors"
	"reflect"
	"testing"
)

func TestMessageFromCopiesPayload(t *testing.T) {
	original := []byte("abc")
	message, err := MessageFrom(original)
	if err != nil || !reflect.DeepEqual(message.Payload, original) { t.Fatalf("message=%+v err=%v", message, err) }
	message.Payload[0] = 'z'
	if original[0] != 'a' { t.Fatal("MessageFrom must own a copy of []byte payload") }
}

func TestTopicBuilders(t *testing.T) {
	topic := Topic{Tenant: "tenant", Namespace: "namespace", Name: "topic"}
	kafkaTopic, err := BuildKafkaTopic(topic)
	if err != nil || kafkaTopic != "tenant.namespace.topic" { t.Fatalf("unexpected kafka topic %q, err=%v", kafkaTopic, err) }
	pulsarTopic, err := BuildPulsarTopic(topic)
	if err != nil || pulsarTopic != "persistent://tenant/namespace/topic" { t.Fatalf("unexpected pulsar topic %q, err=%v", pulsarTopic, err) }
}

func TestMQErrorPreservesCauseAndCode(t *testing.T) {
	cause := errors.New("network down")
	err := NormalizeError(cause, ErrorBackend, "send", BackendKafka)
	var mqErr *MQError
	if !errors.As(err, &mqErr) || !errors.Is(err, cause) { t.Fatalf("error chain lost: %v", err) }
	if mqErr.Code != ErrorBackend || mqErr.Backend != BackendKafka { t.Fatalf("unexpected MQError: %+v", mqErr) }
}
