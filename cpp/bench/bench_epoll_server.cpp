#include <benchmark/benchmark.h>
#include "mental1104/net/epoll_server.h"

#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>

// ---- 小工具 ----
static inline void set_nonblocking(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

// 仅用于公平对比：用 poll 实现一次“读就绪 + 读取”的路径
static inline int poll_read_once(int rfd, int timeout_ms) {
    struct pollfd pfd{rfd, POLLIN, 0};
    int ret = ::poll(&pfd, 1, timeout_ms);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        char buf[256];
        int total = 0;
        for (;;) {
            ssize_t n = ::read(rfd, buf, sizeof(buf));
            if (n > 0) total += (int)n;
            else break;
        }
        return total;
    }
    return 0;
}

// ---------------------------------------------------------------------
// 1) 空轮询开销：测量 dispatch_once(0) 在“无事件”下的调用成本
// ---------------------------------------------------------------------
static void BM_DispatchOnce_Idle(benchmark::State& state) {
    mental1104::EpollServer srv;
    for (auto _ : state) {
        int n = srv.dispatch_once(0);
        benchmark::DoNotOptimize(n);
    }
}
BENCHMARK(BM_DispatchOnce_Idle);

// ---------------------------------------------------------------------
// 2) 读事件吞吐：每次写 1 字节到 pipe，事件驱动回调负责读取
//    统计总读取字节数，反映事件路径吞吐
// ---------------------------------------------------------------------
static void BM_Epoll_Pipe_Read_1B(benchmark::State& state) {
    int fds[2];
    if (::pipe(fds) != 0) { state.SkipWithError("pipe() failed"); return; }
    int rfd = fds[0], wfd = fds[1];
    set_nonblocking(rfd);
    set_nonblocking(wfd);

    mental1104::EpollServer srv;
    std::atomic<size_t> bytes{0};

    srv.add_fd(rfd, EPOLLIN, [&](int fd){
        char buf[256];
        for (;;) {
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0) bytes.fetch_add((size_t)n, std::memory_order_relaxed);
            else break; // EAGAIN or 0
        }
    });

    char c = 'x';
    for (auto _ : state) {
        ssize_t w = ::write(wfd, &c, 1);
        if (w < 0) state.SkipWithError("write failed");
        srv.dispatch_once(0);
    }

    state.counters["bytes"] = (double)bytes.load();
    srv.remove_fd(rfd);
    ::close(wfd);
    ::close(rfd);
}
BENCHMARK(BM_Epoll_Pipe_Read_1B);

// ---------------------------------------------------------------------
// 3) poll 对照组：同样是每次写 1 字节，然后 poll+read；用于与 epoll/kqueue 对比
// ---------------------------------------------------------------------
static void BM_Poll_Pipe_Read_1B(benchmark::State& state) {
    int fds[2];
    if (::pipe(fds) != 0) { state.SkipWithError("pipe() failed"); return; }
    int rfd = fds[0], wfd = fds[1];
    set_nonblocking(rfd);
    set_nonblocking(wfd);

    char c = 'x';
    size_t bytes = 0;
    for (auto _ : state) {
        ssize_t w = ::write(wfd, &c, 1);
        if (w < 0) state.SkipWithError("write failed");
        int got = poll_read_once(rfd, 0);
        bytes += (size_t)got;
    }

    state.counters["bytes"] = (double)bytes;
    ::close(wfd);
    ::close(rfd);
}
BENCHMARK(BM_Poll_Pipe_Read_1B);

// ---------------------------------------------------------------------
// 4) 回调基线：只测 std::function 调用成本，剥离内核事件系统影响
// ---------------------------------------------------------------------
static void BM_CallbackOnly(benchmark::State& state) {
    size_t cnt = 0;
    mental1104::EpollServer::EventCallback cb = [&](int){ ++cnt; };
    for (auto _ : state) {
        cb(-1);
    }
    state.counters["calls"] = (double)cnt;
}
BENCHMARK(BM_CallbackOnly);

BENCHMARK_MAIN();