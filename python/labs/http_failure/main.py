"""本地复现 429/503 与直接断连的 Python HTTP 客户端差异。"""

from __future__ import annotations

import http.client
import socket
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Callable, List, Tuple

from mental1104.network.http_failure import (
    HTTPOutcome,
    observe_http_outcomes,
)


class FailureHandler(BaseHTTPRequestHandler):
    """按路径返回 429、503，或在写入状态行前直接关闭连接。"""

    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:
        """根据请求路径产生本实验需要的三种服务端行为。"""

        if self.path == "/429":
            self._send_empty_response(429)
            return
        if self.path == "/503":
            self._send_empty_response(503)
            return
        if self.path == "/eof":
            # 在 HTTP 状态行出现前关闭连接，使客户端只能得到网络层异常。
            try:
                self.connection.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            self.connection.close()
            self.close_connection = True
            return

        self._send_empty_response(204)

    def log_message(self, format_string: str, *args: object) -> None:
        """关闭 BaseHTTPRequestHandler 的默认标准错误日志，保持实验输出稳定。"""

    def _send_empty_response(self, status_code: int) -> None:
        """发送带 Content-Length: 0 的完整 HTTP 响应。

        Args:
            status_code: 需要返回给客户端的 HTTP 状态码。
        """

        self.send_response(status_code)
        self.send_header("Content-Length", "0")
        self.end_headers()


def request_once(host: str, port: int, path: str) -> http.client.HTTPResponse:
    """通过新的 HTTPConnection 执行一次 GET 请求。

    Args:
        host: 本地实验服务监听地址。
        port: 本地实验服务监听端口。
        path: 429、503 或 eof 场景的请求路径。

    Returns:
        已收到状态行时返回 HTTPResponse。

    Raises:
        http.client.RemoteDisconnected: 服务端在发送状态行前直接断连。
        OSError: 连接建立或读写失败。
    """

    connection = http.client.HTTPConnection(host, port, timeout=2)
    try:
        connection.request("GET", path)
        return connection.getresponse()
    except Exception:
        connection.close()
        raise


def run_scenario(
    request: Callable[[str, int, str], http.client.HTTPResponse],
    host: str,
    port: int,
    scenario: str,
    observed: List[HTTPOutcome],
) -> None:
    """执行单个场景并打印响应可读性、异常和分类结果。

    Args:
        request: 已由 observe_http_outcomes 装饰的 request_once 兼容调用对象。
        host: 本地实验服务监听地址。
        port: 本地实验服务监听端口。
        scenario: 429、503 或 eof。
        observed: 装饰器写入分类结果的列表。
    """

    response = None
    error = None
    try:
        response = request(host, port, "/" + scenario)
    except Exception as exc:
        error = exc

    status_readable = response is not None
    status_code = response.status if response is not None else 0
    if response is not None:
        response.read()
        response.close()

    outcome = observed[-1]
    print(
        "scenario={} request_error={} status_readable={} status_code={} kind={}".format(
            scenario,
            error is not None,
            status_readable,
            status_code,
            outcome.kind.value,
        )
    )


def start_server() -> Tuple[ThreadingHTTPServer, threading.Thread]:
    """启动仅绑定本机随机端口的实验 HTTP 服务。

    Returns:
        HTTPServer 和负责 serve_forever 的 daemon 线程。
    """

    server = ThreadingHTTPServer(("127.0.0.1", 0), FailureHandler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, thread


def main() -> None:
    """运行 429、503 与 EOF 三个场景并打印可观察差异。"""

    server, thread = start_server()
    observed: List[HTTPOutcome] = []
    observed_request = observe_http_outcomes(observed.append)(request_once)
    host, port = server.server_address

    try:
        for scenario in ("429", "503", "eof"):
            run_scenario(observed_request, host, port, scenario, observed)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    main()
