#include "mental1104/util.h"

namespace mental1104 {

/// 按当前 C locale 转换字符，返回独立的小写字符串。
/// @param text 只读输入，不保存视图。
/// @return 小写副本。
std::string to_lower_copy(string_view text) {
  auto lower = [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  };
  std::string out(text.size(), '\0');
  std::transform(text.begin(), text.end(), out.begin(), lower);
  return out;
}

/// 建立带上限的指数退避序列。
/// @param initial 初始等待时间。
/// @param max_delay 后续等待上限。
/// @param factor 每次推进的倍率。
ExponentialBackoff::ExponentialBackoff(std::chrono::milliseconds initial,
                                       std::chrono::milliseconds max_delay,
                                       int factor)
    : initial_(initial), max_(max_delay), factor_(factor), current_(initial) {}

/// 返回本次等待并将内部状态推进到受上限限制的下一次等待。
/// @return 推进前的等待时长。
std::chrono::milliseconds ExponentialBackoff::next() {
  auto delay = this->current_;
  auto next_delay =
      std::chrono::milliseconds(this->current_.count() * this->factor_);
  this->current_ = std::min(next_delay, this->max_);
  return delay;
}

/// 将下一次等待恢复到构造时的初始值。
void ExponentialBackoff::reset() { this->current_ = this->initial_; }

} // namespace mental1104
