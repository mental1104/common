#pragma once

#include "mental1104/log.h"

#include <string>

namespace linkage_test {

/// 独立消费端的可观察结果，不向测试暴露其私有 Key/Hash 或 lambda 类型。
struct Observation {
  std::string normalized;
  int computed;
  bool membership;
  bool parse_ok;
  bool owns_file;
  bool has_random_key;
  int backoff_millis;
  int coroutine_steps;
  bool uses_std_string_view;
};

/// 在另一个翻译单元中调用模板入口和已编译实现。
/// @return 字符串、任务、过滤器、解析和资源管理的可观察结果。
Observation observe_consumer();

/// 从消费端读取日志运行时共享的等级。
/// @return 当前日志等级。
mental1104::LogLevel consumer_log_level();

/// 从消费端输出一条 Info 日志，由库的运行时等级决定是否输出。
void write_consumer_log();

} // namespace linkage_test
