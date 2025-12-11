#include "mental1104/net/epoll_server.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#pragma message("epoll_server: stubbed on Windows; operations are no-ops")
#else
#include <unistd.h>
#if defined(__linux__)
#include <sys/epoll.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#include <sys/time.h>
#else
#error "Unsupported platform: need Linux (epoll) or macOS (kqueue)"
#endif
#endif

namespace mental1104 {

#if defined(__linux__)

// -------------------- Linux: epoll --------------------
EpollServer::EpollServer() : epoll_fd_(::epoll_create1(0)) {
  if (epoll_fd_ == -1)
    throw std::runtime_error("Failed to create epoll fd");
}

EpollServer::~EpollServer() {
  if (epoll_fd_ != -1)
    ::close(epoll_fd_);
}

void EpollServer::add_fd(int fd, uint32_t events, EventCallback cb) {
  epoll_event ev{};
  ev.data.fd = fd;
  ev.events = events;
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1)
    throw std::runtime_error("Failed to add fd to epoll");
  entries_[fd] = Entry{events, std::move(cb)};
}

void EpollServer::modify_fd(int fd, uint32_t events) {
  auto it = entries_.find(fd);
  if (it == entries_.end())
    throw std::runtime_error("modify_fd on unknown fd");
  epoll_event ev{};
  ev.data.fd = fd;
  ev.events = events;
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1)
    throw std::runtime_error("Failed to modify fd in epoll");
  it->second.events = events;
}

void EpollServer::remove_fd(int fd) {
  auto it = entries_.find(fd);
  if (it == entries_.end()) {
    std::cerr << "fd " << fd << " not registered, skip removal\n";
    return;
  }
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
    if (errno == EBADF || errno == ENOENT) {
      std::cerr << "fd " << fd << " invalid or not in epoll, skip removal\n";
    } else {
      std::cerr << "epoll del fail, fd=" << fd
                << ", err=" << std::strerror(errno) << "\n";
    }
  }
  entries_.erase(it);
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
    auto it = entries_.find(fd);
    if (it != entries_.end())
      it->second.cb(fd);
  }
  return n;
}

void EpollServer::event_loop() {
  while (true)
    (void)dispatch_once(-1);
}

#elif defined(__APPLE__)

// -------------------- macOS: kqueue --------------------
static inline void to_timespec(int timeout_ms, ::timespec &ts, ::timespec *&p) {
  if (timeout_ms < 0) {
    p = nullptr;
    return;
  }
  ts.tv_sec = timeout_ms / 1000;
  ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
  p = &ts;
}

EpollServer::EpollServer() : epoll_fd_(::kqueue()) {
  if (epoll_fd_ == -1)
    throw std::runtime_error("Failed to create kqueue fd");
}

EpollServer::~EpollServer() {
  if (epoll_fd_ != -1)
    ::close(epoll_fd_);
}

void EpollServer::add_fd(int fd, uint32_t events, EventCallback cb) {
  struct kevent changes[2];
  int n = 0;
  uint16_t flags = EV_ADD | EV_ENABLE;
  if (events & EPOLLET)
    flags |= EV_CLEAR; // 近似 epoll 边沿

  if (events & EPOLLIN)
    EV_SET(&changes[n++], fd, EVFILT_READ, flags, 0, 0, nullptr);
  if (events & EPOLLOUT)
    EV_SET(&changes[n++], fd, EVFILT_WRITE, flags, 0, 0, nullptr);

  if (n == 0)
    throw std::runtime_error("No events to add");
  if (::kevent(epoll_fd_, changes, n, nullptr, 0, nullptr) == -1)
    throw std::runtime_error("Failed to add fd to kqueue");
  entries_[fd] = Entry{events, std::move(cb)};
}

void EpollServer::modify_fd(int fd, uint32_t events) {
  auto it = entries_.find(fd);
  if (it == entries_.end())
    throw std::runtime_error("modify_fd on unknown fd");
  uint32_t old_events = it->second.events;
  struct kevent changes[4];
  int n = 0;

  if (old_events & EPOLLIN)
    EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  if (old_events & EPOLLOUT)
    EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

  uint16_t flags = EV_ADD | EV_ENABLE;
  if (events & EPOLLET)
    flags |= EV_CLEAR;

  if (events & EPOLLIN)
    EV_SET(&changes[n++], fd, EVFILT_READ, flags, 0, 0, nullptr);
  if (events & EPOLLOUT)
    EV_SET(&changes[n++], fd, EVFILT_WRITE, flags, 0, 0, nullptr);

  if (::kevent(epoll_fd_, changes, n, nullptr, 0, nullptr) == -1)
    throw std::runtime_error("Failed to modify fd in kqueue");
  it->second.events = events;
}

void EpollServer::remove_fd(int fd) {
  auto it = entries_.find(fd);
  if (it == entries_.end()) {
    std::cerr << "fd " << fd << " not registered, skip removal\n";
    return;
  }
  struct kevent changes[2];
  int n = 0;
  uint32_t events = it->second.events;
  if (events & EPOLLIN)
    EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  if (events & EPOLLOUT)
    EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

  if (n > 0 && ::kevent(epoll_fd_, changes, n, nullptr, 0, nullptr) == -1) {
    if (errno == EBADF || errno == ENOENT) {
      // 常见：文件描述符已关闭或从未注册写事件；无需高噪声日志
    } else {
      std::cerr << "kqueue del fail, fd=" << fd
                << ", err=" << std::strerror(errno) << "\n";
    }
  }
  entries_.erase(it);
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
    auto it = entries_.find(fd);
    if (it != entries_.end())
      it->second.cb(fd);
  }
  return n;
}

void EpollServer::event_loop() {
  while (true)
    (void)dispatch_once(-1);
}

#elif defined(_WIN32)

// -------------------- Windows: stub implementations --------------------
EpollServer::EpollServer() : epoll_fd_(-1) {}
EpollServer::~EpollServer() = default;

void EpollServer::add_fd(int, uint32_t, EventCallback) {
  throw std::runtime_error("EpollServer not supported on Windows");
}
void EpollServer::modify_fd(int, uint32_t) {
  throw std::runtime_error("EpollServer not supported on Windows");
}
void EpollServer::remove_fd(int) {}
int EpollServer::dispatch_once(int) { return 0; }
void EpollServer::event_loop() { throw std::runtime_error("EpollServer not supported on Windows"); }

#endif // platform

} // namespace mental1104
