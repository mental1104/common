#ifndef MENTAL1104_C_API_RAII_H
#define MENTAL1104_C_API_RAII_H

#include <cstdio>
#include <type_traits>
#include <utility>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace mental1104 {

#if !defined(_WIN32)
/// 独占 POSIX 文件描述符；无效状态为 -1，系统释放逻辑由库提供。
class unique_fd {
public:
  unique_fd() noexcept : fd_(-1) {}
  explicit unique_fd(int fd) noexcept : fd_(fd) {}

  /// 释放仍由本对象持有的文件描述符；析构不抛异常。
  ~unique_fd() noexcept;

  unique_fd(const unique_fd &) = delete;
  unique_fd &operator=(const unique_fd &) = delete;

  unique_fd(unique_fd &&other) noexcept : fd_(other.release()) {}

  unique_fd &operator=(unique_fd &&other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  int get() const noexcept { return fd_; }

  explicit operator bool() const noexcept { return fd_ >= 0; }

  int release() noexcept {
    const int fd = fd_;
    fd_ = -1;
    return fd;
  }

  /// 释放旧描述符并接管新描述符，相同描述符保持不变。
  /// @param fd 转移所有权的新描述符，-1 表示清空。
  void reset(int fd = -1) noexcept;

private:
  /// 尽力关闭有效描述符，不传播 close 错误。
  void close_current() noexcept;

  int fd_;
};
#endif

/// 独占 C FILE；空指针表示无文件，系统释放逻辑由库提供。
class unique_file {
public:
  unique_file() noexcept : file_(NULL) {}
  explicit unique_file(std::FILE *file) noexcept : file_(file) {}

  /// 关闭仍由本对象持有的 FILE，析构不抛异常。
  ~unique_file() noexcept;

  unique_file(const unique_file &) = delete;
  unique_file &operator=(const unique_file &) = delete;

  unique_file(unique_file &&other) noexcept : file_(other.release()) {}

  unique_file &operator=(unique_file &&other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  std::FILE *get() const noexcept { return file_; }

  explicit operator bool() const noexcept { return file_ != NULL; }

  std::FILE *release() noexcept {
    std::FILE *file = file_;
    file_ = NULL;
    return file;
  }

  /// 关闭旧文件并接管新文件，相同指针保持不变。
  /// @param file 转移所有权的 FILE，NULL 表示清空。
  void reset(std::FILE *file = NULL) noexcept;

private:
  /// 尽力关闭有效文件，不传播 fclose 错误。
  void close_current() noexcept;

  std::FILE *file_;
};

/// 打开 C 文件并返回独占所有权包装。
/// @param path 非空文件路径，不保存调用方指针。
/// @param mode 非空 fopen 模式。
/// @return 成功时持有文件；失败时为空包装。
unique_file open_file(const char *path, const char *mode) noexcept;

// Runs a cleanup callback once when leaving scope unless dismissed.
template <typename Callback> class scope_exit {
public:
  typedef Callback callback_type;

  explicit scope_exit(callback_type callback) noexcept(
      std::is_nothrow_move_constructible<callback_type>::value)
      : callback_(std::move(callback)), active_(true) {}

  ~scope_exit() noexcept {
    if (active_) {
      try {
        callback_();
      } catch (...) {
      }
    }
  }

  scope_exit(const scope_exit &) = delete;
  scope_exit &operator=(const scope_exit &) = delete;

  scope_exit(scope_exit &&other) noexcept(
      std::is_nothrow_move_constructible<callback_type>::value)
      : callback_(std::move(other.callback_)), active_(other.active_) {
    other.dismiss();
  }

  scope_exit &operator=(scope_exit &&) = delete;

  void dismiss() noexcept { active_ = false; }
  void release() noexcept { dismiss(); }

  bool active() const noexcept { return active_; }

private:
  callback_type callback_;
  bool active_;
};

template <typename Callback>
scope_exit<typename std::decay<Callback>::type>
make_scope_exit(Callback &&callback) noexcept(
    noexcept(scope_exit<typename std::decay<Callback>::type>(
        std::forward<Callback>(callback)))) {
  typedef typename std::decay<Callback>::type callback_type;
  return scope_exit<callback_type>(std::forward<Callback>(callback));
}

} // namespace mental1104

#endif // MENTAL1104_C_API_RAII_H
