package mental1104

import (
	"errors"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

// roundTripperFunc 让测试可以用函数构造最小 http.RoundTripper。
type roundTripperFunc func(request *http.Request) (*http.Response, error)

// RoundTrip 调用测试函数并原样返回其 response/error。
func (fn roundTripperFunc) RoundTrip(request *http.Request) (*http.Response, error) {
	return fn(request)
}

// TestClassifyHTTPOutcome 验证成功、HTTP 状态失败、网络失败和非法 nil 组合的稳定语义。
func TestClassifyHTTPOutcome(t *testing.T) {
	networkErr := errors.New("connection reset")
	tests := []struct {
		name       string
		response   *http.Response
		err        error
		kind       HTTPOutcomeKind
		statusCode int
		outcomeErr error
	}{
		{
			name:       "success response",
			response:   &http.Response{StatusCode: http.StatusNoContent},
			kind:       HTTPOutcomeSuccess,
			statusCode: http.StatusNoContent,
		},
		{
			name:       "429 remains an HTTP status failure",
			response:   &http.Response{StatusCode: http.StatusTooManyRequests},
			kind:       HTTPOutcomeStatusFailure,
			statusCode: http.StatusTooManyRequests,
		},
		{
			name:       "503 remains an HTTP status failure",
			response:   &http.Response{StatusCode: http.StatusServiceUnavailable},
			kind:       HTTPOutcomeStatusFailure,
			statusCode: http.StatusServiceUnavailable,
		},
		{
			name:       "transport error wins over a non nil response",
			response:   &http.Response{StatusCode: http.StatusBadGateway},
			err:        networkErr,
			kind:       HTTPOutcomeNetworkFailure,
			outcomeErr: networkErr,
		},
		{
			name:       "nil response without error is a network failure",
			kind:       HTTPOutcomeNetworkFailure,
			outcomeErr: ErrNilHTTPResponse,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			outcome := ClassifyHTTPOutcome(test.response, test.err)
			if outcome.Kind != test.kind {
				t.Fatalf("Kind = %v, want %v", outcome.Kind, test.kind)
			}
			if outcome.StatusCode != test.statusCode {
				t.Fatalf("StatusCode = %d, want %d", outcome.StatusCode, test.statusCode)
			}
			if !errors.Is(outcome.Err, test.outcomeErr) {
				t.Fatalf("Err = %v, want %v", outcome.Err, test.outcomeErr)
			}
		})
	}
}

// TestHTTPClientDo429503AndEOF 对比 client.Do 在 HTTP 状态失败和直接断连时的返回值。
func TestHTTPClientDo429503AndEOF(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
		switch request.URL.Path {
		case "/429":
			writer.WriteHeader(http.StatusTooManyRequests)
		case "/503":
			writer.WriteHeader(http.StatusServiceUnavailable)
		case "/eof":
			hijacker, ok := writer.(http.Hijacker)
			if !ok {
				t.Error("test server does not support connection hijacking")
				return
			}
			connection, _, err := hijacker.Hijack()
			if err != nil {
				t.Errorf("Hijack() error = %v", err)
				return
			}
			_ = connection.Close()
		default:
			writer.WriteHeader(http.StatusNoContent)
		}
	}))
	defer server.Close()

	tests := []struct {
		name         string
		path         string
		wantDoError  bool
		wantResponse bool
		wantStatus   int
		wantKind     HTTPOutcomeKind
	}{
		{
			name:         "429",
			path:         "/429",
			wantResponse: true,
			wantStatus:   http.StatusTooManyRequests,
			wantKind:     HTTPOutcomeStatusFailure,
		},
		{
			name:         "503",
			path:         "/503",
			wantResponse: true,
			wantStatus:   http.StatusServiceUnavailable,
			wantKind:     HTTPOutcomeStatusFailure,
		},
		{
			name:        "EOF",
			path:        "/eof",
			wantDoError: true,
			wantKind:    HTTPOutcomeNetworkFailure,
		},
	}

	client := server.Client()
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			request, err := http.NewRequest(http.MethodGet, server.URL+test.path, nil)
			if err != nil {
				t.Fatalf("NewRequest() error = %v", err)
			}

			response, doErr := client.Do(request)
			if response != nil {
				defer response.Body.Close()
			}

			if (doErr != nil) != test.wantDoError {
				t.Fatalf("client.Do() error = %v, want error = %v", doErr, test.wantDoError)
			}
			if (response != nil) != test.wantResponse {
				t.Fatalf("response present = %v, want %v", response != nil, test.wantResponse)
			}
			if response != nil && response.StatusCode != test.wantStatus {
				t.Fatalf("StatusCode = %d, want %d", response.StatusCode, test.wantStatus)
			}

			outcome := ClassifyHTTPOutcome(response, doErr)
			if outcome.Kind != test.wantKind {
				t.Fatalf("outcome.Kind = %v, want %v", outcome.Kind, test.wantKind)
			}
		})
	}
}

// TestWrapHTTPTransport 验证包装器会上报分类结果，同时保持原始 response/error 不变。
func TestWrapHTTPTransport(t *testing.T) {
	request, err := http.NewRequest(http.MethodGet, "http://example.test/resource", nil)
	if err != nil {
		t.Fatalf("NewRequest() error = %v", err)
	}

	wantResponse := &http.Response{
		StatusCode: http.StatusServiceUnavailable,
		Body:       io.NopCloser(strings.NewReader("busy")),
		Request:    request,
	}
	var observedRequest *http.Request
	var observedOutcome HTTPOutcome
	wrapped := WrapHTTPTransport(
		roundTripperFunc(func(gotRequest *http.Request) (*http.Response, error) {
			if gotRequest != request {
				t.Fatalf("request pointer changed")
			}
			return wantResponse, nil
		}),
		func(gotRequest *http.Request, outcome HTTPOutcome) {
			observedRequest = gotRequest
			observedOutcome = outcome
		},
	)

	gotResponse, gotErr := wrapped.RoundTrip(request)
	if gotErr != nil {
		t.Fatalf("RoundTrip() error = %v", gotErr)
	}
	if gotResponse != wantResponse {
		t.Fatalf("response pointer changed")
	}
	if observedRequest != request {
		t.Fatalf("observer request pointer changed")
	}
	if observedOutcome.Kind != HTTPOutcomeStatusFailure || observedOutcome.StatusCode != http.StatusServiceUnavailable {
		t.Fatalf("observed outcome = %+v", observedOutcome)
	}
}
