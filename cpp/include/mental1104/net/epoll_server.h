#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>

#if defined(__linux__)
// Linux: 给使用方（比如 bench/test）提供 EPOLL* 宏
#  include <sys/epoll.h>
#else
// 非 Linux：提供占位常量，kqueue 实现层会映射
#ifndef EPOLLIN
#define EPOLLIN   0x001u
#endif
#ifndef EPOLLOUT
#define EPOLLOUT  0x004u
#endif
#ifndef EPOLLET
#define EPOLLET   0x80000000u
#endif
#endif

namespace mental1104 {

class EpollServer {
public:
    using EventCallback = std::function<void(int)>;
    struct Entry {
        uint32_t events;
        EventCallback cb;
    };

    EpollServer();
    ~EpollServer();

    EpollServer(const EpollServer&)            = delete;
    EpollServer& operator=(const EpollServer&) = delete;

    void add_fd(int fd, uint32_t events, EventCallback cb);
    void modify_fd(int fd, uint32_t events);
    void remove_fd(int fd);

    void event_loop();                          // 阻塞循环
    int  dispatch_once(int timeout_ms = -1);    // 单次分发：返回处理的事件数量

private:
    int epoll_fd_;  // Linux: epoll fd; macOS: kqueue fd
    std::unordered_map<int, Entry> entries_;
};

} // namespace mental1104
