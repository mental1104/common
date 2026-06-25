#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>

#if defined(__linux__)
// Linux: 给使用方（比如 bench/test）提供 EPOLL* 宏
#include <sys/epoll.h>
#else
// 非 Linux：提供占位常量，kqueue 实现层会映射
#ifndef EPOLLIN
#define EPOLLIN 0x001u // 占位：表示“读事件”，让非 Linux 也能使用 epoll 风格接口
#endif
#ifndef EPOLLOUT
#define EPOLLOUT 0x004u // 占位：表示“写事件”
#endif
#ifndef EPOLLET
#define EPOLLET 0x80000000u // 占位：表示“边缘触发模式”；不带 EPOLLET 时默认是水平触发模式
#endif
#endif

namespace mental1104 {

class EpollServer { // 非线程安全：add_fd/remove_fd/set_callback/dispatch_once 不能并发调用，跨线程使用需由调用方加锁或用 eventfd 唤醒事件线程
public:
  using EventCallback = std::function<void(int)>; // 事件回调：事件循环只通知 fd 就绪，不需要回调返回值；传 int 是把就绪的 fd 交给使用方读写
  struct Entry { // 每个被监听 fd 的登记信息：监听哪些事件，以及事件触发时执行哪个回调
    uint32_t events; // 保存事件掩码，例如 EPOLLIN、EPOLLOUT、EPOLLIN | EPOLLET
    EventCallback cb; // 保存“fd 就绪后要执行的处理逻辑”；fd 本身只表示对象，不能表示要做什么
  };

  EpollServer();
  ~EpollServer() noexcept; // 析构只释放底层事件 fd，不向外抛异常；失败最多记录日志

  EpollServer(const EpollServer &) = delete;
  EpollServer &operator=(const EpollServer &) = delete;

  void add_fd(int fd, uint32_t events, EventCallback cb = {}); // upsert：新增 fd 时必须提供 cb；已注册 fd 传空 cb 表示只改 events，传非空 cb 表示同时替换回调
  void set_callback(int fd, EventCallback cb); // 只替换已注册 fd 的回调，不修改底层监听事件
  void remove_fd(int fd);

  void event_loop();                      // 阻塞循环
  int dispatch_once(int timeout_ms = -1); // 单次分发：返回处理的事件数量

private:
  void set_callback_entry(int fd, EventCallback cb); // 内部复用：只写 entries_ 里的回调，调用前由 public 接口决定 fd 是否必须已注册
  int epoll_fd_; // Linux: epoll fd; macOS: kqueue fd
  std::unordered_map<int, Entry> entries_; // key 是被监听的 fd，value 是该 fd 对应的事件掩码和回调
};

} // namespace mental1104
