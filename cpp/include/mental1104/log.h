#ifndef MENTAL1104_LOG_H
#define MENTAL1104_LOG_H

// Simple logging utility: prefers spdlog when available, falls back to stdout/stderr.
#if defined(__has_include)
#  if __has_include(<spdlog/spdlog.h>)
#    ifndef SPDLOG_HEADER_ONLY
#      define SPDLOG_HEADER_ONLY
#    endif
#    include <spdlog/spdlog.h>
#    define M1104_HAS_SPDLOG 1
#  else
#    define M1104_HAS_SPDLOG 0
#  endif
#else
#  define M1104_HAS_SPDLOG 0
#endif

#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <atomic>
#include <cstdlib>
#include <cctype>
#include <cstdio>

#if defined(__has_include)
#  if __has_include(<format>)
#    include <format>
#    ifndef __cpp_lib_format
#      define __cpp_lib_format 201907L
#    endif
#  endif
#endif

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#  define M1104_HAS_STD_FORMAT 1
#else
#  define M1104_HAS_STD_FORMAT 0
#endif

namespace mental1104 {

// 可用枚举值越低表示越详细，便于比较
enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

#ifndef M1104_LOG_DEFAULT_LEVEL
#define M1104_LOG_DEFAULT_LEVEL "info"
#endif

namespace detail {
inline const char *level_name(LogLevel lvl) {
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

#if M1104_HAS_SPDLOG
inline spdlog::level::level_enum to_spd_level(LogLevel lvl);
#endif

inline LogLevel level_from_string(std::string_view text) {
  auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
  std::string tmp;
  tmp.reserve(text.size());
  for (char c : text) {
    tmp.push_back(lower(c));
  }

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

inline LogLevel default_level() {
  return level_from_string(M1104_LOG_DEFAULT_LEVEL);
}

inline LogLevel env_level() {
  if (const char *env = std::getenv("MENTAL1104_LOG_LEVEL")) {
    if (*env != '\0') {
      return level_from_string(env);
    }
  }
  return default_level();
}

inline std::atomic<LogLevel> &global_level() {
  static std::atomic<LogLevel> lvl{env_level()};
#if M1104_HAS_SPDLOG
  static bool init_spdlog = [] {
    spdlog::set_level(detail::to_spd_level(lvl.load(std::memory_order_relaxed)));
    return true;
  }();
  (void)init_spdlog;
#endif
  return lvl;
}

inline int level_rank(LogLevel lvl) { return static_cast<int>(lvl); }

template <typename... Args> std::string to_string(Args &&...args) {
  std::ostringstream oss;
  (oss << ... << std::forward<Args>(args));
  return oss.str();
}

#if M1104_HAS_SPDLOG
inline spdlog::level::level_enum to_spd_level(LogLevel lvl) {
  switch (lvl) {
  case LogLevel::Debug:
    return spdlog::level::debug;
  case LogLevel::Info:
    return spdlog::level::info;
  case LogLevel::Warning:
    return spdlog::level::warn;
  case LogLevel::Error:
    return spdlog::level::err;
  }
  return spdlog::level::info;
}
#endif

template <typename... Args>
std::string format_printf(std::string_view fmt, Args &&...args) {
  std::string fmt_cstr(fmt); // ensure null-terminated
  int size = std::snprintf(nullptr, 0, fmt_cstr.c_str(),
                           std::forward<Args>(args)...);
  if (size <= 0) {
    return to_string(fmt, " ", std::forward<Args>(args)...);
  }
  std::string buf(static_cast<size_t>(size), '\0');
  std::snprintf(buf.data(), static_cast<size_t>(size) + 1, fmt_cstr.c_str(),
                std::forward<Args>(args)...);
  return buf;
}

inline bool has_brace_format(std::string_view fmt) {
  return fmt.find('{') != std::string_view::npos;
}

inline bool has_printf_format(std::string_view fmt) {
  return fmt.find('%') != std::string_view::npos;
}

template <typename... Args>
std::string format_flexible(std::string_view fmt, Args &&...args) {
#if M1104_HAS_STD_FORMAT
  if (has_brace_format(fmt)) {
    return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
  }
#endif
  if (has_printf_format(fmt)) {
    return format_printf(fmt, std::forward<Args>(args)...);
  }
#if M1104_HAS_STD_FORMAT
  return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
#else
  // 回退：既没有 <format> 也没有 printf 样式时，拼接以避免编译失败
  return to_string(fmt, " ", std::forward<Args>(args)...);
#endif
}
} // namespace detail

inline LogLevel get_log_level() {
  return detail::global_level().load(std::memory_order_relaxed);
}

inline void set_log_level(LogLevel lvl) {
  detail::global_level().store(lvl, std::memory_order_relaxed);
#if M1104_HAS_SPDLOG
  spdlog::set_level(detail::to_spd_level(lvl));
#endif
}

template <typename... Args> inline void log(LogLevel level, Args &&...args) {
  if (detail::level_rank(level) < detail::level_rank(get_log_level())) {
    return;
  }
  auto msg = detail::to_string(std::forward<Args>(args)...);
#if M1104_HAS_SPDLOG
  spdlog::log(detail::to_spd_level(level), msg);
#else
  auto &out = (level == LogLevel::Error) ? std::cerr : std::cout;
  out << "[" << detail::level_name(level) << "] " << msg << std::endl;
#endif
}

template <typename... Args>
inline void logf(LogLevel level, std::string_view fmt, Args &&...args) {
  if (detail::level_rank(level) < detail::level_rank(get_log_level())) {
    return;
  }
  auto msg = detail::format_flexible(fmt, std::forward<Args>(args)...);
#if M1104_HAS_SPDLOG
  spdlog::log(detail::to_spd_level(level), msg);
#else
  auto &out = (level == LogLevel::Error) ? std::cerr : std::cout;
  out << "[" << detail::level_name(level) << "] " << msg << std::endl;
#endif
}

#define M1104_LOG_DEBUG(...)                                                   \
  ::mental1104::log(::mental1104::LogLevel::Debug, __VA_ARGS__)
#define M1104_LOG_INFO(...)                                                    \
  ::mental1104::log(::mental1104::LogLevel::Info, __VA_ARGS__)
#define M1104_LOG_WARNING(...)                                                 \
  ::mental1104::log(::mental1104::LogLevel::Warning, __VA_ARGS__)
#define M1104_LOG_ERROR(...)                                                   \
  ::mental1104::log(::mental1104::LogLevel::Error, __VA_ARGS__)

#define M1104_LOG_DEBUGF(fmt, ...)                                             \
  ::mental1104::logf(::mental1104::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define M1104_LOG_INFOF(fmt, ...)                                              \
  ::mental1104::logf(::mental1104::LogLevel::Info, fmt, ##__VA_ARGS__)
#define M1104_LOG_WARNINGF(fmt, ...)                                           \
  ::mental1104::logf(::mental1104::LogLevel::Warning, fmt, ##__VA_ARGS__)
#define M1104_LOG_ERRORF(fmt, ...)                                             \
  ::mental1104::logf(::mental1104::LogLevel::Error, fmt, ##__VA_ARGS__)

} // namespace mental1104

#endif // MENTAL1104_LOG_H
