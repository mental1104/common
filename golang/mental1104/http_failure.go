package mental1104

import (
	"errors"
	"net/http"
)

// ErrNilHTTPResponse 表示调用方在没有 error 的情况下传入了 nil HTTP 响应。
// 正常的 net/http RoundTripper 不应产生该组合，因此它被归类为网络层失败。
var ErrNilHTTPResponse = errors.New("http outcome: response is nil without an error")

// HTTPOutcomeKind 表示一次 HTTP 调用最终落在哪一层。
// 它只区分成功、已收到 HTTP 响应但状态失败，以及未获得有效 HTTP 响应的网络失败。
type HTTPOutcomeKind uint8

const (
	// HTTPOutcomeSuccess 表示请求获得了状态码小于 400 的有效 HTTP 响应。
	HTTPOutcomeSuccess HTTPOutcomeKind = iota
	// HTTPOutcomeStatusFailure 表示请求获得了状态码大于等于 400 的有效 HTTP 响应。
	HTTPOutcomeStatusFailure
	// HTTPOutcomeNetworkFailure 表示请求在获得有效 HTTP 响应前发生了传输错误。
	HTTPOutcomeNetworkFailure
)

// String 返回稳定的结果类别文本，便于日志、指标标签和实验输出使用。
// 未知枚举值返回 "unknown"。
func (kind HTTPOutcomeKind) String() string {
	switch kind {
	case HTTPOutcomeSuccess:
		return "success"
	case HTTPOutcomeStatusFailure:
		return "http_status_failure"
	case HTTPOutcomeNetworkFailure:
		return "network_failure"
	default:
		return "unknown"
	}
}

// HTTPOutcome 描述一次 HTTP 调用的可观察结果。
// StatusCode 仅在已经获得 HTTP 响应时有效；Err 仅在网络层失败时非 nil。
type HTTPOutcome struct {
	Kind       HTTPOutcomeKind
	StatusCode int
	Err        error
}

// HTTPOutcomeObserver 接收被包装 Transport 的调用结果。
// request 是本次 RoundTrip 的原始请求；observer 可能被并发调用，调用方应自行保证线程安全。
type HTTPOutcomeObserver func(request *http.Request, outcome HTTPOutcome)

// ClassifyHTTPOutcome 根据 net/http 返回的 response 和 error 区分 HTTP 层失败与网络层失败。
// err 非 nil 时网络层失败优先，即使 response 也非 nil；这与 http.Client.Do 的错误处理约定一致。
// response 非 nil 且 err 为 nil 时，状态码大于等于 400 归类为 HTTP 层失败，其他状态归类为成功。
func ClassifyHTTPOutcome(response *http.Response, err error) HTTPOutcome {
	if err != nil {
		return HTTPOutcome{
			Kind: HTTPOutcomeNetworkFailure,
			Err:  err,
		}
	}

	// nil response 与 nil error 违反 RoundTripper 契约，按“未获得有效 HTTP 响应”处理。
	if response == nil {
		return HTTPOutcome{
			Kind: HTTPOutcomeNetworkFailure,
			Err:  ErrNilHTTPResponse,
		}
	}

	if response.StatusCode >= http.StatusBadRequest {
		return HTTPOutcome{
			Kind:       HTTPOutcomeStatusFailure,
			StatusCode: response.StatusCode,
		}
	}

	return HTTPOutcome{
		Kind:       HTTPOutcomeSuccess,
		StatusCode: response.StatusCode,
	}
}

// WrapHTTPTransport 将结果观察逻辑包装到现有 http.RoundTripper 外层。
// next 为 nil 时使用 http.DefaultTransport；observer 为 nil 时仍保持请求行为不变但不会发送观察事件。
// 包装器不读取或关闭响应体，也不会把 4xx/5xx 转换为 Go error，因此调用方仍可读取 resp.StatusCode。
func WrapHTTPTransport(next http.RoundTripper, observer HTTPOutcomeObserver) http.RoundTripper {
	if next == nil {
		next = http.DefaultTransport
	}

	return &httpOutcomeTransport{
		next:     next,
		observer: observer,
	}
}

// httpOutcomeTransport 在不改变 RoundTripper 返回值的前提下上报调用结果。
type httpOutcomeTransport struct {
	next     http.RoundTripper
	observer HTTPOutcomeObserver
}

// RoundTrip 调用下游 Transport，并把同一组 response/error 交给分类器。
// 返回值原样透传；observer 的 panic 不会被恢复，因此 observer 应保持轻量且不可抛出 panic。
func (transport *httpOutcomeTransport) RoundTrip(request *http.Request) (*http.Response, error) {
	response, err := transport.next.RoundTrip(request)
	if transport.observer != nil {
		transport.observer(request, ClassifyHTTPOutcome(response, err))
	}
	return response, err
}
