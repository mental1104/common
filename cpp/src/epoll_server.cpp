#include "mental1104/net/epoll_server.h"

#include "mental1104/log.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#if defined(_WIN32)
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

void EpollServer::set_callback_entry(int fd, EventCallback cb) {
  if (!cb)
    throw std::runtime_error("callback must not be empty");
  this->entries_[fd].cb = std::move(cb);
}

#if defined(__linux__)

// -------------------- Linux: epoll --------------------
EpollServer::EpollServer() : epoll_fd_(::epoll_create1(0)) { // 构造时创建一个 epoll 实例，返回值保存到底层 epoll fd
  if (this->epoll_fd_ == -1)                                // -1 表示创建失败
    throw std::runtime_error("Failed to create epoll fd");   // 创建失败则抛异常，避免对象处于不可用状态
}

EpollServer::~EpollServer() noexcept {
  if (this->epoll_fd_ != -1 && ::close(this->epoll_fd_) == -1) { // 有效 fd 才关闭；析构不抛异常，close 失败只记录日志
    int close_errno = errno; // 先保存 errno，避免后续函数调用覆盖失败原因
    M1104_LOG_ERROR("Failed to close epoll fd ", this->epoll_fd_,
                    ": errno=", close_errno, ", ",
                    std::strerror(close_errno));
  }
}

void EpollServer::add_fd(int fd, uint32_t events, EventCallback cb) {
  auto it = this->entries_.find(fd);
  bool registered = it != this->entries_.end();
  if (!registered && !cb)
    throw std::runtime_error("add_fd on new fd requires callback");

  epoll_event ev{}; // epoll_ctl 需要的事件描述结构，{} 会把字段初始化为 0
  ev.data.fd = fd;  // 告诉 epoll：事件触发后把这个 fd 原样带回来
  ev.events = events; // 要监听的事件掩码，例如 EPOLLIN、EPOLLOUT、EPOLLET
  int op = registered ? EPOLL_CTL_MOD : EPOLL_CTL_ADD; // 已注册则改事件，未注册则新增，实现 add_fd 的 upsert 语义
  if (::epoll_ctl(this->epoll_fd_, op, fd, &ev) == -1) // 把业务 fd 注册或更新到底层 epoll 实例
    throw std::runtime_error("Failed to add fd to epoll");  // 失败则抛异常，调用方需要处理
  if (registered) {
    it->second.events = events; // 已注册 fd 始终更新事件掩码
    if (cb)
      this->set_callback_entry(fd, std::move(cb)); // 已注册 fd 只有传入非空 cb 时才替换回调
  } else {
    this->entries_[fd].events = events; // 先写事件掩码，再复用回调设置逻辑
    this->set_callback_entry(fd, std::move(cb)); // 本地保存 fd 对应的回调；cb 形参已按值接收，移动进 Entry 可避免再拷贝 std::function
  }
}

void EpollServer::set_callback(int fd, EventCallback cb) { // 只替换本地回调，不调用 epoll_ctl，因为监听事件没有变化
  auto it = this->entries_.find(fd);
  if (it == this->entries_.end())
    throw std::runtime_error("set_callback on unknown fd");
  this->set_callback_entry(fd, std::move(cb));
}

void EpollServer::remove_fd(int fd) {
  auto it = this->entries_.find(fd);
  if (it == this->entries_.end()) {
    M1104_LOG_WARNING("fd ", fd, " not registered, skip removal");
    return;
  }
  if (::epoll_ctl(this->epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
    if (errno == EBADF || errno == ENOENT) {
      M1104_LOG_WARNING("fd ", fd, " invalid or not in epoll, skip removal");
    } else {
      int del_errno = errno;
      M1104_LOG_ERROR("epoll del fail, fd=", fd, ", errno=", del_errno, ", ",
                      std::strerror(del_errno));
    }
  }
  this->entries_.erase(it);
}

int EpollServer::dispatch_once(int timeout_ms) {
  constexpr int MAX_EVENTS = 16;
  epoll_event events[MAX_EVENTS];
  int n = ::epoll_wait(this->epoll_fd_, events, MAX_EVENTS, timeout_ms);
  if (n == -1) {
    int wait_errno = errno;
    M1104_LOG_ERROR("epoll_wait error: errno=", wait_errno, ", ",
                    std::strerror(wait_errno));
    return 0;
  }
  for (int i = 0; i < n; ++i) {
    int fd = events[i].data.fd;
    auto it = this->entries_.find(fd);
    if (it != this->entries_.end())
      it->second.cb(fd);
  }
  return n;
}

void EpollServer::event_loop() {
  while (true)
    (void)this->dispatch_once(-1);
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

static void update_kqueue_events(int kq, int fd, uint32_t old_events,
                                 uint32_t events) {
  struct kevent changes[4];
  int n = 0;

  if (old_events & EPOLLIN)
    EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  if (old_events & EPOLLOUT)
    EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

  uint16_t flags = EV_ADD | EV_ENABLE;
  if (events & EPOLLET)
    flags |= EV_CLEAR; // 近似 epoll 边沿

  if (events & EPOLLIN)
    EV_SET(&changes[n++], fd, EVFILT_READ, flags, 0, 0, nullptr);
  if (events & EPOLLOUT)
    EV_SET(&changes[n++], fd, EVFILT_WRITE, flags, 0, 0, nullptr);

  if (n == 0)
    throw std::runtime_error("No events to add");
  if (::kevent(kq, changes, n, nullptr, 0, nullptr) == -1)
    throw std::runtime_error("Failed to add fd to kqueue");
}

EpollServer::EpollServer() : epoll_fd_(::kqueue()) {
  if (this->epoll_fd_ == -1)
    throw std::runtime_error("Failed to create kqueue fd");
}

EpollServer::~EpollServer() noexcept {
  if (this->epoll_fd_ != -1 && ::close(this->epoll_fd_) == -1) { // 有效 fd 才关闭；析构不抛异常，close 失败只记录日志
    int close_errno = errno; // 先保存 errno，避免后续函数调用覆盖失败原因
    M1104_LOG_ERROR("Failed to close kqueue fd ", this->epoll_fd_,
                    ": errno=", close_errno, ", ",
                    std::strerror(close_errno));
  }
}

void EpollServer::add_fd(int fd, uint32_t events, EventCallback cb) {
  auto it = this->entries_.find(fd);
  bool registered = it != this->entries_.end();
  if (!registered && !cb)
    throw std::runtime_error("add_fd on new fd requires callback");

  update_kqueue_events(this->epoll_fd_, fd, registered ? it->second.events : 0, events); // 新增或更新底层 kqueue 监听事件
  if (registered) {
    it->second.events = events; // 已注册 fd 始终更新事件掩码
    if (cb)
      this->set_callback_entry(fd, std::move(cb)); // 已注册 fd 只有传入非空 cb 时才替换回调
  } else {
    this->entries_[fd].events = events; // 先写事件掩码，再复用回调设置逻辑
    this->set_callback_entry(fd, std::move(cb));
  }
}

void EpollServer::set_callback(int fd, EventCallback cb) { // 只替换本地回调，不调用 kevent，因为监听事件没有变化
  auto it = this->entries_.find(fd);
  if (it == this->entries_.end())
    throw std::runtime_error("set_callback on unknown fd");
  this->set_callback_entry(fd, std::move(cb));
}

void EpollServer::remove_fd(int fd) {
  auto it = this->entries_.find(fd);
  if (it == this->entries_.end()) {
    M1104_LOG_WARNING("fd ", fd, " not registered, skip removal");
    return;
  }
  struct kevent changes[2];
  int n = 0;
  uint32_t events = it->second.events;
  if (events & EPOLLIN)
    EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  if (events & EPOLLOUT)
    EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

  if (n > 0 && ::kevent(this->epoll_fd_, changes, n, nullptr, 0, nullptr) == -1) {
    if (errno == EBADF || errno == ENOENT) {
      // 常见：文件描述符已关闭或从未注册写事件；无需高噪声日志
    } else {
      int del_errno = errno;
      M1104_LOG_ERROR("kqueue del fail, fd=", fd, ", errno=", del_errno, ", ",
                      std::strerror(del_errno));
    }
  }
  this->entries_.erase(it);
}

int EpollServer::dispatch_once(int timeout_ms) {
  constexpr int MAX_EVENTS = 16;
  struct kevent evlist[MAX_EVENTS];
  ::timespec ts, *pts = nullptr;
  to_timespec(timeout_ms, ts, pts);

  int n = ::kevent(this->epoll_fd_, nullptr, 0, evlist, MAX_EVENTS, pts);
  if (n == -1) {
    int wait_errno = errno;
    M1104_LOG_ERROR("kevent wait error: errno=", wait_errno, ", ",
                    std::strerror(wait_errno));
    return 0;
  }
  for (int i = 0; i < n; ++i) {
    int fd = static_cast<int>(evlist[i].ident);
    auto it = this->entries_.find(fd);
    if (it != this->entries_.end())
      it->second.cb(fd);
  }
  return n;
}

void EpollServer::event_loop() {
  while (true)
    (void)this->dispatch_once(-1);
}

#elif defined(_WIN32)

// -------------------- Windows: stub implementations --------------------
EpollServer::EpollServer() : epoll_fd_(-1) {}
EpollServer::~EpollServer() noexcept = default;

void EpollServer::add_fd(int, uint32_t, EventCallback) {
  throw std::runtime_error("EpollServer not supported on Windows");
}
void EpollServer::set_callback(int, EventCallback) {
  throw std::runtime_error("EpollServer not supported on Windows");
}
void EpollServer::remove_fd(int) {}
int EpollServer::dispatch_once(int) { return 0; }
void EpollServer::event_loop() { throw std::runtime_error("EpollServer not supported on Windows"); }

#endif // platform

} // namespace mental1104
