#include "mental1104/c_api_raii.h"

namespace mental1104 {

#if !defined(_WIN32)

/// 释放仍由本对象持有的文件描述符；析构不抛异常。
unique_fd::~unique_fd() noexcept { this->reset(); }

/// 释放旧描述符并接管新描述符，相同描述符保持不变。
/// @param fd 转移所有权的新描述符，-1 表示清空。
void unique_fd::reset(int fd) noexcept {
  if (this->fd_ != fd) {
    this->close_current();
    this->fd_ = fd;
  }
}

/// 尽力关闭有效描述符，不传播 close 错误。
void unique_fd::close_current() noexcept {
  if (this->fd_ >= 0) {
    (void)::close(this->fd_);
  }
}

#endif // !defined(_WIN32)

/// 关闭仍由本对象持有的 FILE，析构不抛异常。
unique_file::~unique_file() noexcept { this->reset(); }

/// 关闭旧文件并接管新文件，相同指针保持不变。
/// @param file 转移所有权的 FILE，NULL 表示清空。
void unique_file::reset(std::FILE *file) noexcept {
  if (this->file_ != file) {
    this->close_current();
    this->file_ = file;
  }
}

/// 尽力关闭有效文件，不传播 fclose 错误。
void unique_file::close_current() noexcept {
  if (this->file_ != NULL) {
    (void)std::fclose(this->file_);
  }
}

/// 打开 C 文件并返回独占所有权包装。
/// @param path 非空文件路径，不保存调用方指针。
/// @param mode 非空 fopen 模式。
/// @return 成功时持有文件；失败时为空包装。
unique_file open_file(const char *path, const char *mode) noexcept {
#if defined(_MSC_VER)
  std::FILE *file = NULL;
  if (fopen_s(&file, path, mode) != 0) {
    file = NULL;
  }
  return unique_file(file);
#else
  return unique_file(std::fopen(path, mode));
#endif
}

} // namespace mental1104
