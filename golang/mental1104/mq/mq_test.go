package mq

import (
	"reflect"
	"testing"
)

func TestMarshalRecord(t *testing.T) {
	got, err := MarshalRecord(map[string]int{"value": 7})
	if err != nil || string(got) != `{"value":7}` {
		t.Fatalf("unexpected JSON payload %q, err=%v", got, err)
	}
	original := []byte("abc")
	got, err = MarshalRecord(original)
	if err != nil || !reflect.DeepEqual(got, original) {
		t.Fatalf("unexpected bytes payload %q, err=%v", got, err)
	}
	got[0] = 'z'
	if original[0] != 'a' {
		t.Fatal("MarshalRecord must copy byte slices")
	}
}

func TestTopicBuilders(t *testing.T) {
	kafkaTopic, err := BuildKafkaTopic("tenant", "namespace", "topic")
	if err != nil || kafkaTopic != "tenant.namespace.topic" {
		t.Fatalf("unexpected kafka topic %q, err=%v", kafkaTopic, err)
	}
	pulsarTopic, err := BuildPulsarTopic("tenant", "namespace", "topic")
	if err != nil || pulsarTopic != "persistent://tenant/namespace/topic" {
		t.Fatalf("unexpected pulsar topic %q, err=%v", pulsarTopic, err)
	}
	if _, err := BuildKafkaTopic("", "", ""); err == nil {
		t.Fatal("empty kafka topic should fail")
	}
	if _, err := BuildPulsarTopic("", "namespace", "topic"); err == nil {
		t.Fatal("empty pulsar tenant should fail")
	}
}
