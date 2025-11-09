#include "mental1104/net/epoll_server.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#if defined(__linux__)
  #include <sys/epoll.h>
#elif defined(__APPLE__)
  #include <sys/event.h>
  #include <sys/time.h>
#else
  #error "Unsupported platform: need Linux (epoll) or macOS (kqueue)"
#endif

namespace mental1104 {

#if defined(__linux__)

// -------------------- Linux: epoll --------------------
EpollServer::EpollServer() : epoll_fd_(::epoll_create1(0)) {
    if (epoll_fd_ == -1) throw std::runtime_error("Failed to create epoll fd");
}

EpollServer::~EpollServer() {
    if (epoll_fd_ != -1) ::close(epoll_fd_);
}

void EpollServer::add_fd(int fd, uint32_t events, EventCallback cb) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events  = events;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1)
        throw std::runtime_error("Failed to add fd to epoll");
    callbacks_[fd] = std::move(cb);
}

void EpollServer::modify_fd(int fd, uint32_t events) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events  = events;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1)
        throw std::runtime_error("Failed to modify fd in epoll");
}

void EpollServer::remove_fd(int fd) {
    if (!callbacks_.count(fd)) {
        std::cerr << "fd " << fd << " not registered, skip removal\n";
        return;
    }
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        if (errno == EBADF || errno == ENOENT) {
            std::cerr << "fd " << fd << " invalid or not in epoll, skip removal\n";
        } else {
            std::cerr << "epoll del fail, fd=" << fd << ", err=" << std::strerror(errno) << "\n";
        }
    } else {
        std::cout << "epoll del ok, fd=" << fd << "\n";
    }
    callbacks_.erase(fd);
}

int EpollServer::dispatch_once(int timeout_ms) {
    constexpr int MAX_EVENTS = 16;
    epoll_event events[MAX_EVENTS];
    int n = ::epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout_ms);
    if (n == -1) {
        std::cerr << "epoll_wait error: " << std::strerror(errno) << "\n";
        return 0;
    }
    for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;
        auto it = callbacks_.find(fd);
        if (it != callbacks_.end()) it->second(fd);
    }
    return n;
}

void EpollServer::event_loop() {
    while (true) (void)dispatch_once(-1);
}

#elif defined(__APPLE__)

// -------------------- macOS: kqueue --------------------
static inline void to_timespec(int timeout_ms, ::timespec& ts, ::timespec*& p) {
    if (timeout_ms < 0) { p = nullptr; return; }
    ts.tv_sec  = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
    p = &ts;
}

EpollServer::EpollServer() : epoll_fd_(::kqueue()) {
    if (epoll_fd_ == -1) throw std::runtime_error("Failed to create kqueue fd");
}

EpollServer::~EpollServer() {
    if (epoll_fd_ != -1) ::close(epoll_fd_);
}

void EpollServer::add_fd(int fd, uint32_t events, EventCallback cb) {
    struct kevent changes[2];
    int n = 0;
    uint16_t flags = EV_ADD | EV_ENABLE;
    if (events & EPOLLET) flags |= EV_CLEAR; // 近似 epoll 边沿

    if (events & EPOLLIN)  EV_SET(&changes[n++], fd, EVFILT_READ,  flags, 0, 0, nullptr);
    if (events & EPOLLOUT) EV_SET(&changes[n++], fd, EVFILT_WRITE, flags, 0, 0, nullptr);

    if (n == 0) throw std::runtime_error("No events to add");
    if (::kevent(epoll_fd_, changes, n, nullptr, 0, nullptr) == -1)
        throw std::runtime_error("Failed to add fd to kqueue");
    callbacks_[fd] = std::move(cb);
}

void EpollServer::modify_fd(int fd, uint32_t events) {
    struct kevent changes[4];
    int n = 0;

    EV_SET(&changes[n++], fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

    uint16_t flags = EV_ADD | EV_ENABLE;
    if (events & EPOLLET) flags |= EV_CLEAR;

    if (events & EPOLLIN)  EV_SET(&changes[n++], fd, EVFILT_READ,  flags, 0, 0, nullptr);
    if (events & EPOLLOUT) EV_SET(&changes[n++], fd, EVFILT_WRITE, flags, 0, 0, nullptr);

    if (::kevent(epoll_fd_, changes, n, nullptr, 0, nullptr) == -1)
        throw std::runtime_error("Failed to modify fd in kqueue");
}

void EpollServer::remove_fd(int fd) {
    if (!callbacks_.count(fd)) {
        std::cerr << "fd " << fd << " not registered, skip removal\n";
        return;
    }
    struct kevent changes[2];
    int n = 0;
    EV_SET(&changes[n++], fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

    if (::kevent(epoll_fd_, changes, n, nullptr, 0, nullptr) == -1) {
        if (errno == EBADF || errno == ENOENT) {
            std::cerr << "fd " << fd << " invalid or not in kqueue, skip removal\n";
        } else {
            std::cerr << "kqueue del fail, fd=" << fd << ", err=" << std::strerror(errno) << "\n";
        }
    } else {
        std::cout << "kqueue del ok, fd=" << fd << "\n";
    }
    callbacks_.erase(fd);
}

int EpollServer::dispatch_once(int timeout_ms) {
    constexpr int MAX_EVENTS = 16;
    struct kevent evlist[MAX_EVENTS];
    ::timespec ts, *pts = nullptr;
    to_timespec(timeout_ms, ts, pts);

    int n = ::kevent(epoll_fd_, nullptr, 0, evlist, MAX_EVENTS, pts);
    if (n == -1) {
        std::cerr << "kevent wait error: " << std::strerror(errno) << "\n";
        return 0;
    }
    for (int i = 0; i < n; ++i) {
        int fd = static_cast<int>(evlist[i].ident);
        auto it = callbacks_.find(fd);
        if (it != callbacks_.end()) it->second(fd);
    }
    return n;
}

void EpollServer::event_loop() {
    while (true) (void)dispatch_once(-1);
}

#endif // platform

} // namespace mental1104