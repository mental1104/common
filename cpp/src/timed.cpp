#include "mental1104/timed.h"

#include <iomanip>
#include <iostream>

namespace mental1104 {
namespace detail {

/// 输出进入消息；这是计时器的用户可见输出，不转成日志等级消息。
/// @param name 当前调用期间有效的显示名称。
void print_timed_enter(const std::string &name) {
  std::cout << "Entering " << name << '\n';
}

/// 输出耗时并沿用原来的流格式设置。
/// @param label 名称与 with 前缀，不保存引用。
/// @param seconds 执行耗时，单位为秒。
void print_timed_exit(const std::string &label, double seconds) {
  std::cout << "Exiting " << label << std::fixed << std::setprecision(10)
            << seconds << " seconds" << std::endl;
}

} // namespace detail
} // namespace mental1104
