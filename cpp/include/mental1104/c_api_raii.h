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
// Owns a POSIX file descriptor. Invalid state is -1.
class unique_fd {
public:
  unique_fd() noexcept : fd_(-1) {}
  explicit unique_fd(int fd) noexcept : fd_(fd) {}

  ~unique_fd() noexcept { reset(); }

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

  void reset(int fd = -1) noexcept {
    if (fd_ != fd) {
      close_current();
      fd_ = fd;
    }
  }

private:
  void close_current() noexcept {
    if (fd_ >= 0) {
      (void)::close(fd_);
    }
  }

  int fd_;
};
#endif

// Owns a C FILE*. Invalid state is nullptr.
class unique_file {
public:
  unique_file() noexcept : file_(NULL) {}
  explicit unique_file(std::FILE *file) noexcept : file_(file) {}

  ~unique_file() noexcept { reset(); }

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

  void reset(std::FILE *file = NULL) noexcept {
    if (file_ != file) {
      close_current();
      file_ = file;
    }
  }

private:
  void close_current() noexcept {
    if (file_ != NULL) {
      (void)std::fclose(file_);
    }
  }

  std::FILE *file_;
};

inline unique_file open_file(const char *path, const char *mode) noexcept {
  return unique_file(std::fopen(path, mode));
}

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
make_scope_exit(Callback &&callback) noexcept(noexcept(
    scope_exit<typename std::decay<Callback>::type>(
        std::forward<Callback>(callback)))) {
  typedef typename std::decay<Callback>::type callback_type;
  return scope_exit<callback_type>(std::forward<Callback>(callback));
}

} // namespace mental1104

#endif // MENTAL1104_C_API_RAII_H
