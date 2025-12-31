package hello

import "testing"

func TestExtractWorld(t *testing.T) {
	got, ok := ExtractWorld(Hello)
	if !ok {
		t.Fatalf("expected to find world")
	}
	if got != "world" {
		t.Fatalf("got %q, want %q", got, "world")
	}
}

func TestExtractWorldMissing(t *testing.T) {
	if _, ok := ExtractWorld("no match"); ok {
		t.Fatalf("expected missing world")
	}
}
