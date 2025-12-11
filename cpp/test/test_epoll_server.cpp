/*
 * @Date: 2025-11-09 14:09:51
 * @Author: mental1104 mental1104@gmail.com
 * @LastEditors: mental1104 mental1104@gmail.com
 * @LastEditTime: 2025-11-09 14:09:58
 */
#include <gtest/gtest.h>

#ifdef _WIN32
TEST(EpollServerTest, SkipOnWindows) {
  GTEST_SKIP() << "EpollServer not supported on Windows";
}
#else

#include "mental1104/net/epoll_server.h"

#include <atomic>
#include <sys/types.h>
#include <unistd.h> // pipe, read, write, close

using mental1104::EpollServer;

TEST(EpollServerTest, ReadableTriggersCallback) {
  int fds[2];
  ASSERT_EQ(::pipe(fds), 0) << "pipe() failed";
  int rfd = fds[0], wfd = fds[1];

  EpollServer srv;
  std::atomic<int> called{0};

  srv.add_fd(rfd, EPOLLIN, [&](int fd) {
    char buf[64];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n > 0) {
      called.fetch_add(1, std::memory_order_relaxed);
    }
  });

  const char *msg = "hi";
  ASSERT_EQ(::write(wfd, msg, 2), 2) << "write() failed";

  int n = srv.dispatch_once(1000); // 最多等 1s
  EXPECT_GE(n, 1);
  EXPECT_GE(called.load(std::memory_order_relaxed), 1);

  srv.remove_fd(rfd);
  ::close(wfd);
  ::close(rfd);
}

TEST(EpollServerTest, WriteEndIsWritable) {
  int fds[2];
  ASSERT_EQ(::pipe(fds), 0);
  int rfd = fds[0], wfd = fds[1];

  EpollServer srv;
  std::atomic<int> called{0};

  // 监控写端可写：在 pipe 未写满的情况下应立即就绪（Linux: EPOLLOUT；macOS:
  // EVFILT_WRITE）
  srv.add_fd(wfd, EPOLLOUT,
             [&](int) { called.fetch_add(1, std::memory_order_relaxed); });

  int n = srv.dispatch_once(200); // 200ms 足够
  EXPECT_GE(n, 1);
  EXPECT_GE(called.load(std::memory_order_relaxed), 1);

  srv.remove_fd(wfd);
  ::close(wfd);
  ::close(rfd);
}

TEST(EpollServerTest, RemoveClosedFdDoesNotThrow) {
  int fds[2];
  ASSERT_EQ(::pipe(fds), 0);
  int rfd = fds[0], wfd = fds[1];

  EpollServer srv;
  srv.add_fd(rfd, EPOLLIN, [&](int) {});

  ::close(rfd); // 先关闭，再移除（应容错）
  EXPECT_NO_THROW(srv.remove_fd(rfd));

  ::close(wfd);
}

TEST(EpollServerTest, RemoveUnregisteredFdIsNoop) {
  EpollServer srv;
  // 未注册直接移除，应该只是输出告警，不抛异常
  EXPECT_NO_THROW(srv.remove_fd(123456)); // 一个明显不合法/未注册的 fd
}

#endif // _WIN32
