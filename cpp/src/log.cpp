#include "mental1104/log.h"

namespace mental1104 {

/// 返回日志等级的静态名称。
/// @param lvl 日志等级。
/// @return 无需释放的静态字符串，未知等级为 UNKNOWN。
const char *detail::level_name(LogLevel lvl) {
  switch (lvl) {
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warning:
    return "WARNING";
  case LogLevel::Error:
    return "ERROR";
  }
  return "UNKNOWN";
}

/// 解析不区分大小写的日志等级名称。
/// @param text 只读等级文本，不保存视图。
/// @return 解析结果；未知名称返回 Info。
LogLevel detail::level_from_string(string_view text) {
  // 全局复用的快速小写化逻辑抽到了
  // util::to_lower_copy，响应“快速转换大小写是很多场景都会用到的需求”，避免在这里内联重复实现。
  // 显式带上 mental1104 命名空间，兜底调用公共 util 版本。
  std::string tmp = mental1104::to_lower_copy(text);

  if (tmp == "debug" || tmp == "dbg")
    return LogLevel::Debug;
  if (tmp == "info")
    return LogLevel::Info;
  if (tmp == "warning" || tmp == "warn")
    return LogLevel::Warning;
  if (tmp == "error" || tmp == "err")
    return LogLevel::Error;
  return LogLevel::Info;
}

/// 读取构建日志库时指定的默认日志等级。
/// @return 编译配置对应的等级。
LogLevel detail::default_level() {
  return level_from_string(M1104_LOG_DEFAULT_LEVEL);
}

/// 读取 MENTAL1104_LOG_LEVEL，缺失或为空时使用库的默认等级。
/// @return 初始日志等级。
LogLevel detail::env_level() {
  // getenv 可能返回 nullptr；这里先判 env 非空，再判首字符非 '\0'
  // 才解引用，避免用户担心的空指针解引用。
#if defined(_MSC_VER)
  char *env = NULL;
  size_t len = 0;
  if (_dupenv_s(&env, &len, "MENTAL1104_LOG_LEVEL") == 0 && env != NULL) {
    std::string text(env);
    std::free(env);
    if (!text.empty()) {
      return level_from_string(text);
    }
  }
#else
  if (const char *env = std::getenv("MENTAL1104_LOG_LEVEL")) {
    if (*env != '\0') {
      return level_from_string(env);
    }
  }
#endif
  return default_level();
}

/// 取得由日志库统一初始化的原子等级，并同步可选 spdlog 后端。
/// @return 库持有的等级引用，调用方不负责释放。
std::atomic<LogLevel> &detail::global_level() {
  static std::atomic<LogLevel> lvl{env_level()};
#if M1104_HAS_SPDLOG
  // 静态 lambda 只在首次进入 global_level 时执行一次；如果直接把 set_level
  // 写在函数体，每次调用 global_level 都会重复设置 spdlog 等级。
  // 也可以用逗号表达式初始化静态 bool，但 lambda 让逻辑块更清晰。
  static bool init_spdlog = [] {
    // lvl 是静态原子，读取时用 memory_order_relaxed：这里只需要取值初始化
    // spdlog，不依赖任何同步顺序，避免无谓的栅栏/开销。
    // 如果不传该参数会走默认的
    // memory_order_seq_cst，语义上也能工作，但会施加更强的顺序/栅栏，带来不必要的开销。
    spdlog::set_level(
        detail::to_spd_level(lvl.load(std::memory_order_relaxed)));
    return true;
  }();
  (void)init_spdlog;
#endif
  return lvl;
}

#if M1104_HAS_SPDLOG

/// 将公共日志等级映射到编译时选定的 spdlog 后端。
/// @param lvl 公共等级。
/// @return 对应 spdlog 等级。
spdlog::level::level_enum detail::to_spd_level(LogLevel lvl) {
  // 适配层：将内部 LogLevel 映射到 spdlog::level
  // 枚举，集中封装避免在调用处直接写 spdlog 命名空间。
  switch (lvl) {
  case LogLevel::Debug:
    return spdlog::level::debug; // DEBUG -> spdlog debug
  case LogLevel::Info:
    return spdlog::level::info; // INFO -> spdlog info
  case LogLevel::Warning:
    return spdlog::level::warn; // WARNING -> spdlog warn
  case LogLevel::Error:
    return spdlog::level::err; // ERROR -> spdlog err
  }
  return spdlog::level::info; // 默认兜底 info
}

#endif

/// 读取日志库当前等级。
/// @return 当前过滤阈值。
LogLevel get_log_level() {
  // 读取当前全局日志等级（relaxed 原子读取）。
  return detail::global_level().load(std::memory_order_relaxed);
}

/// 更新日志库等级并同步可选后端。
/// @param lvl 新的过滤阈值。
void set_log_level(LogLevel lvl) {
  // 更新全局等级；若启用 spdlog 同步其全局 level。
  detail::global_level().store(lvl, std::memory_order_relaxed);
#if M1104_HAS_SPDLOG
  spdlog::set_level(detail::to_spd_level(lvl));
#endif
}

/// 输出已经完成类型适配和格式化的消息，不再次过滤等级。
/// @param level 消息等级；Error 的回退输出使用 stderr，其余使用 stdout。
/// @param message 当前调用期间有效的只读消息，不保存引用。
void detail::write_log_message(LogLevel level, const std::string &message) {
#if M1104_HAS_SPDLOG
  spdlog::log(detail::to_spd_level(level), message);
#else
  auto &out = (level == LogLevel::Error) ? std::cerr : std::cout;
  out << "[" << detail::level_name(level) << "] " << message << std::endl;
#endif
}

} // namespace mental1104
