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

// ======================== 多fd·稀疏就绪：体现epoll优势 ========================
#include <vector>
#include <random>

// 创建 N 个非阻塞 pipe，返回 {rfd[], wfd[]}
static bool make_pipes(size_t N, std::vector<int>& rfds, std::vector<int>& wfds) {
    rfds.resize(N); wfds.resize(N);
    for (size_t i = 0; i < N; ++i) {
        int fds[2];
        if (::pipe(fds) != 0) return false;
        set_nonblocking(fds[0]);
        set_nonblocking(fds[1]);
        rfds[i] = fds[0];
        wfds[i] = fds[1];
    }
    return true;
}

static void close_pipes(std::vector<int>& rfds, std::vector<int>& wfds) {
    for (int fd : rfds) ::close(fd);
    for (int fd : wfds) ::close(fd);
    rfds.clear(); wfds.clear();
}

// -------- EPOLL：N个fd长期注册，仅H个fd在每轮被写入触发（稀疏活跃集） --------
static void BM_Epoll_ManyFds_Sparse(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0)); // fd 总数
    const size_t H = static_cast<size_t>(state.range(1)); // 每轮热 fd 数
    std::vector<int> rfds, wfds;
    if (!make_pipes(N, rfds, wfds)) { state.SkipWithError("pipe() failed"); return; }

    mental1104::EpollServer srv;
    std::atomic<size_t> bytes{0};

    // ET：一次唤醒尽量多干活；回调里必须读到EAGAIN
    for (size_t i = 0; i < N; ++i) {
        int rfd = rfds[i];
        srv.add_fd(rfd, EPOLLIN | EPOLLET, [&](int fd){
            char buf[4096];
            for (;;) {
                ssize_t n = ::read(fd, buf, sizeof(buf));
                if (n > 0) bytes.fetch_add((size_t)n, std::memory_order_relaxed);
                else break; // EAGAIN/0/ERR 都退出；非阻塞下OK
            }
        });
    }

    // 固定选择策略：轮转挑选 H 个 fd，避免随机数开销干扰
    size_t cursor = 0;
    char one = 'x';

    for (auto _ : state) {
        for (size_t k = 0; k < H; ++k) {
            size_t idx = (cursor + k) % N;
            ssize_t w = ::write(wfds[idx], &one, 1);
            if (w < 0) { state.SkipWithError("write failed"); goto OUT; }
        }
        cursor += H;
        // 批处理一次：只返回当前活跃集（~H），不受 N 影响
        srv.dispatch_once(0);
    }

OUT:
    state.counters["fds"]   = static_cast<double>(N);
    state.counters["hot"]   = static_cast<double>(H);
    state.counters["bytes"] = static_cast<double>(bytes.load());

    for (size_t i = 0; i < N; ++i) srv.remove_fd(rfds[i]);
    close_pipes(rfds, wfds);
}
BENCHMARK(BM_Epoll_ManyFds_Sparse)->Args({1024, 8})->Args({4096, 8})->Args({8192, 8});

// -------- POLL：同样N个fd常驻，但每轮都要扫描N个pollfd（O(N)） --------
static void BM_Poll_ManyFds_Sparse(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    const size_t H = static_cast<size_t>(state.range(1));
    std::vector<int> rfds, wfds;
    if (!make_pipes(N, rfds, wfds)) { state.SkipWithError("pipe() failed"); return; }

    std::vector<pollfd> pfds(N);
    for (size_t i = 0; i < N; ++i) { pfds[i].fd = rfds[i]; pfds[i].events = POLLIN; pfds[i].revents = 0; }

    size_t cursor = 0;
    char one = 'x';
    size_t bytes = 0;

    for (auto _ : state) {
        for (size_t k = 0; k < H; ++k) {
            size_t idx = (cursor + k) % N;
            ssize_t w = ::write(wfds[idx], &one, 1);
            if (w < 0) { state.SkipWithError("write failed"); goto OUT; }
        }
        cursor += H;

        // 扫描整个 N 集合（O(N)）
        int n = ::poll(pfds.data(), pfds.size(), 0);
        if (n < 0) { state.SkipWithError("poll failed"); goto OUT; }

        // 读取就绪项：循环读到EAGAIN，避免残留造成下轮重复就绪
        char buf[4096];
        for (size_t i = 0; i < N && n > 0; ++i) {
            if (pfds[i].revents & POLLIN) {
                for (;;) {
                    ssize_t r = ::read(pfds[i].fd, buf, sizeof(buf));
                    if (r > 0) bytes += (size_t)r;
                    else break;
                }
                --n;
            }
            pfds[i].revents = 0; // 清就绪位
        }
    }

OUT:
    state.counters["fds"]   = static_cast<double>(N);
    state.counters["hot"]   = static_cast<double>(H);
    state.counters["bytes"] = static_cast<double>(bytes);

    close_pipes(rfds, wfds);
}
BENCHMARK(BM_Poll_ManyFds_Sparse)->Args({1024, 8})->Args({4096, 8})->Args({8192, 8});


BENCHMARK_MAIN();