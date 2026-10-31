package main

import (
	"fmt"
	"net/http"
	"net/http/httptest"

	"github.com/mental1104/common/golang/mental1104"
)

// main 启动本地测试服务，并使用 client.Do 对比 429、503 和直接断连三种结果。
func main() {
	server := httptest.NewServer(http.HandlerFunc(func(writer http.ResponseWriter, request *http.Request) {
		switch request.URL.Path {
		case "/429":
			writer.WriteHeader(http.StatusTooManyRequests)
		case "/503":
			writer.WriteHeader(http.StatusServiceUnavailable)
		case "/eof":
			closeWithoutResponse(writer)
		default:
			writer.WriteHeader(http.StatusNoContent)
		}
	}))
	defer server.Close()

	client := server.Client()
	for _, scenario := range []string{"429", "503", "eof"} {
		runScenario(client, server.URL, scenario)
	}
}

// runScenario 执行一次请求并打印 client.Do 的 error、response 可读性和分类结果。
// client 和 baseURL 必须指向仍在运行的本地测试服务；scenario 是 429、503 或 eof。
func runScenario(client *http.Client, baseURL string, scenario string) {
	request, err := http.NewRequest(http.MethodGet, baseURL+"/"+scenario, nil)
	if err != nil {
		panic(err)
	}

	response, doErr := client.Do(request)
	statusReadable := response != nil
	statusCode := 0
	if response != nil {
		statusCode = response.StatusCode
		_ = response.Body.Close()
	}
	outcome := mental1104.ClassifyHTTPOutcome(response, doErr)

	fmt.Printf(
		"scenario=%s do_error=%t status_readable=%t status_code=%d kind=%s\n",
		scenario,
		doErr != nil,
		statusReadable,
		statusCode,
		outcome.Kind.String(),
	)
}

// closeWithoutResponse 劫持当前连接并在写入 HTTP 状态行之前直接关闭，稳定制造 EOF。
// writer 必须支持 http.Hijacker；不支持或劫持失败时函数会 panic，因为实验前提已经不成立。
func closeWithoutResponse(writer http.ResponseWriter) {
	hijacker, ok := writer.(http.Hijacker)
	if !ok {
		panic("HTTP server does not support hijacking")
	}
	connection, _, err := hijacker.Hijack()
	if err != nil {
		panic(err)
	}
	_ = connection.Close()
}
