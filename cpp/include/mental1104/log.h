// WALKTHROUGH: done
#ifndef MENTAL1104_LOG_H
#define MENTAL1104_LOG_H

// Pull in compiler feature macros early because M1104_HAS_INCLUDE is used
// below.
#include "mental1104/meta/compiler_support.h"
#include "mental1104/meta/format_support.h"
#include "mental1104/util.h"

// Simple logging utility: prefers spdlog when available, falls back to
// stdout/stderr. M1104_HAS_INCLUDE wraps __has_include：编译期按正常 include
// 搜索链探测头存在性，支持 C++17 正式特性，低于 C++17
// 时若编译器不扩展该特性则直接为 0（走回退路径）。
#if M1104_HAS_INCLUDE(<spdlog / spdlog.h>) // 这里实际检测 spdlog
                                           // 是否可用，若可用则启用 spdlog
                                           // 路径，否则走后面的 stdout/stderr
                                           // 回退
#ifndef SPDLOG_HEADER_ONLY
#define SPDLOG_HEADER_ONLY // 头文件模式：实现随头展开，免链接依赖，编译慢一些；库模式：需链接
                           // libspdlog，编译快但要处理链接路径/版本
#endif
#include <spdlog/spdlog.h>
#define M1104_HAS_SPDLOG 1
#else
#define M1104_HAS_SPDLOG 0
#endif

#if M1104_HAS_INCLUDE(<fmt / ranges.h>)
#include <fmt/ranges.h>
#define M1104_HAS_FMT_RANGES 1
#else
#define M1104_HAS_FMT_RANGES 0
#endif

#if M1104_HAS_INCLUDE(<fmt / ostream.h>)
#include <fmt/ostream.h>
#define M1104_HAS_FMT_OSTREAM 1
#else
#define M1104_HAS_FMT_OSTREAM 0
#endif

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <forward_list>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#if M1104_HAS_CXX20
#include <ranges>
#endif

#if M1104_HAS_CXX20
#include <source_location>
#else
struct no_source_location {
}; // 占位类型：在没有 std::source_location
   // 时用它作为形参/默认值的类型替身，使函数签名不变（调用侧仍可不传此参数）
#endif

namespace mental1104 {
#if M1104_HAS_STRING_VIEW
using string_view = std::string_view; // 由 M1104_HAS_STRING_VIEW 控制：C++17
                                      // 起用 std::string_view
#else
using string_view = std::string; // C++11/14 回退到 std::string 保持接口一致
#endif
namespace log_detail {

// 第二模板参数给一个默认的
// void，让这个形参成为“可替换失败点”：主模板默认认为没有 size()。
// 下方特化把该形参替换成 void_t<decltype(...size())>，若表达式合法则匹配特化为
// true，否则替换失败回落到主模板而非编译错误（典型 SFINAE 用法）。 调用时只写
// has_size_method<Container>，第二参数始终走默认值，用于区分主模板/特化而不需显式传参。
// 此处使用 declval<const T&>()，可同时匹配左值/右值调用 size()。
// 只匹配右值时使用std::declval<T>()即可。只有编译时开销没有运行时开销
template <typename T, typename = void>
struct has_size_method : std::false_type {};
#if M1104_HAS_CXX17
template <typename T>
struct has_size_method<T,
                       std::void_t<decltype(std::declval<const T &>().size())>>
    : std::true_type {};
#endif

template <typename T> struct is_sequence : std::false_type {};
// 这里只做显式特化枚举：默认假，特定 STL
// 容器（vector/list/forward_list）被标记为 true，不依赖 SFINAE 自动判定。
// SFINAE
// 要求在替换模板形参时表达式非法仅导致该候选被丢弃，这里没有待替换的依赖表达式，因此不算
// SFINAE。
template <typename T> struct is_sequence<std::vector<T>> : std::true_type {};
template <typename T> struct is_sequence<std::list<T>> : std::true_type {};
template <typename T>
struct is_sequence<std::forward_list<T>> : std::true_type {};

template <typename T> struct is_map_like : std::false_type {};
// 显式支持 std::map 与 std::unordered_map，其余（multimap/unordered_multimap
// 等）默认视为 false，需时可自行添加特化。
template <typename K, typename V>
struct is_map_like<std::map<K, V>> : std::true_type {};
template <typename K, typename V>
struct is_map_like<std::unordered_map<K, V>> : std::true_type {};

#if !M1104_HAS_CXX17
// 无 C++17 时用 std::distance 计算元素个数；std::distance
// 返回有符号差值，若迭代器顺序颠倒会为负，但正常容器 begin/end
// 保证非负，随后转换为 size_t。 size_t
// 为平台相关的无符号大小类型（通常等于指针宽度，32/64 位差异），在
// <cstddef>/<cstdlib> 均可获得，本文件已有 <cstdlib>
// 保证可用。跨平台只要保持“内存里的计数/索引”语义不会有问题，序列化或强转到更窄类型时才可能截断。
template <typename Container> size_t distance_size(const Container &c) {
  return static_cast<size_t>(std::distance(c.begin(), c.end()));
}

// 低于 C++17 时不能用 if constexpr，需要用两条重载 + 表达式 SFINAE
// 来区分是否存在 size()。 设计思路： 1) int 版在重载决议中更优先，签名尝试形成
// decltype(c.size())，成功则输出 c.size()。 2) 若 c.size()
// 不存在/不可访问，替换失败触发 SFINAE，int 版被丢弃，落入 long 版回退到
// distance_size。 3) 无参包装只传 0，触发上述优先级，无需调用侧显式区分。
template <typename Container>
auto print_size_fallback(const Container &c, std::ostream &out,
                         int) -> decltype(c.size(), void()) {
  out << "(size: " << c.size() << ") " << std::endl;
}

template <typename Container>
void print_size_fallback(const Container &c, std::ostream &out, long) {
  out << "(size: " << distance_size(c) << ") " << std::endl;
}

template <typename Container>
void print_size_fallback(const Container &c, std::ostream &out) {
  print_size_fallback(c, out, 0); // 传 0 优先匹配 int 重载，若替换失败（无
                                  // size()）再落到 long 版；调用方无需显式传参
}
#endif

template <typename Container, typename SourceLocation>
// 输出可选的调试信息（文件/行和容器大小），根据是否有 C++20 source_location 和
// size() 能力分层编译。
void maybe_print_info(const Container &c, bool show_info, std::ostream &out,
                      SourceLocation loc) {
  (void)loc;
#if M1104_HAS_CXX20
  if (show_info) {
    out << "[File: " << loc.file_name() << ", Line: " << loc.line()
        << "] "; // file_name() 由 std::source_location
                 // 捕获，一般是编译期源路径，是否是绝对/相对取决于编译器设置（例如相对路径保留原始包含时的相对形态）
    // 编译命令传入的路径形态决定结果：传绝对路径或启用如 GCC/Clang 的
    // -ffile-prefix-map/-fmacro-prefix-map 会改写；MSVC /FC 会强制绝对路径。
    // 例：在源码根运行 g++ main.cpp... 则 file_name() 可能是相对的
    // "main.cpp"；在构建目录用绝对路径 g++ /home/user/proj/main.cpp... 或加
    // -ffile-prefix-map=/home/user/proj=src 则输出绝对路径或被替换后的路径。 if
    // constexpr 在编译期选择分支：仅当容器检测到有 size()
    // 方法时才编译调用，否则跳过该块以避免无成员错误；若换成普通
    // if，即便条件为假也会检查 c.size() 的语义，容器缺少 size()
    // 会导致编译失败。 if constexpr 才能在编译期裁掉不适用的分支。
    if constexpr (has_size_method<Container>::value) {
      out << "(size: " << c.size() << ") " << std::endl;
    }
  }
#elif M1104_HAS_CXX17
  // C++17 分支：没有 source_location，只打印 size（若可用）。
  if (show_info) {
    if constexpr (has_size_method<Container>::value) {
      out << "(size: " << c.size() << ") " << std::endl;
    }
  }
#else
  if (show_info) {
    print_size_fallback(c, out);
  }
#endif
}

template <typename Container, typename SourceLocation>
void format_sequence(const Container &c, bool show_info, std::ostream &out,
                     SourceLocation loc) {
  // 按顺序打印容器元素：先输出可选的文件/行与 size，再以 {a, b, c}
  // 形式遍历元素。
  maybe_print_info(c, show_info, out, loc);
  out << "{";
#if M1104_HAS_CXX20 && M1104_HAS_FMT_RANGES && M1104_HAS_FMT_OSTREAM
  // C++20 + fmt 支持时，直接把元素包装成 streamed 视图，无需中间 string
  // 容器/stringstream。
  auto streamed = c | std::views::transform(
                          [](const auto &elem) { return fmt::streamed(elem); });
  out << fmt::format("{}", fmt::join(streamed, ", "));
#elif M1104_HAS_CXX20 && M1104_HAS_FMT_RANGES
  // 只有 fmt/ranges 时，用字符串中间容器配合 fmt::join（不依赖 fmt/ostream）。
  std::vector<std::string> elems;
  if constexpr (has_size_method<Container>::value) {
    elems.reserve(static_cast<size_t>(c.size()));
  }
  // 用泛型算法填充中间字符串容器，避免手写循环；复用 ostringstream
  // 降低重复构造开销。 这里用 std::ranges::transform（C++20 ranges
  // 版），相比传统 std::transform 更强调约束/借用模型并支持管道式组合，接口接受
  // range 而非裸迭代器。
  std::ostringstream oss;
  std::ranges::transform(
      c, std::back_inserter(elems), [&oss](const auto &element) {
        // 重置并复用流：清空缓冲与状态，写入元素并提取字符串。
        oss.str(std::string()); // str() 赋空字符串以清除内部缓冲内容
        oss.clear();            // clear()
                     // 重置流状态标志（eof/fail），避免上次写入状态影响后续
        oss << element;
        return oss.str();
      });
  out << fmt::format("{}", fmt::join(elems, ", "));
#else
  bool first = true;
  // 也可用算法如 std::for_each/std::copy 搭配
  // ostream_iterator，但仍需处理分隔符的首元素特判；当前手写循环最简洁。
  for (const auto &element : c) {
    if (!first)
      out << ", ";
    out << element;
    first = false;
  }
#endif
  out << "}" << std::endl;
}

#if M1104_HAS_CXX17

template <typename Map> auto sorted_map_items(const Map &m) {
  using Key = typename Map::key_type;
  using Value = typename Map::mapped_type;
  std::vector<std::pair<Key, const Value *>> items;
  items.reserve(m.size());
  for (const auto &kv : m)
    items.emplace_back(kv.first, &kv.second);
  std::sort(items.begin(), items.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  return items;
}

// indent_width 允许上层控制缩进宽度（默认 4），避免魔数散布。
template <typename K, typename V, typename SourceLocation>
void format_map_entry(const K &key, const V &value, bool is_first_element,
                      int indent_level, std::ostream &out, SourceLocation loc,
                      int indent_width = 4) {
  if (!is_first_element) {
    out << ",\n";
  }
  out << std::string(indent_level * indent_width, ' ') << "\"" << key << "\": ";
  if constexpr (is_map_like<V>::value) {
    out << "{\n";
    bool nested_first = true;
    for (const auto &kv : sorted_map_items(value)) {
      format_map_entry(kv.first, *kv.second, nested_first, indent_level + 1,
                       out, loc, indent_width);
      nested_first = false;
    }
    out << "\n" << std::string(indent_level * indent_width, ' ') << "}";
  } else {
    out << "\"" << value << "\"";
  }
}

template <typename K, typename V, typename SourceLocation>
void format_map_like(const std::map<K, V> &m, bool show_info, std::ostream &out,
                     SourceLocation loc, int indent_width = 4) {
  maybe_print_info(m, show_info, out, loc);
  out << "{\n";
  bool first = true;
  for (const auto &kv : sorted_map_items(m)) {
    format_map_entry(kv.first, *kv.second, first, 1, out, loc, indent_width);
    first = false;
  }
  out << "\n}" << std::endl;
}

template <typename K, typename V, typename SourceLocation>
void format_map_like(const std::unordered_map<K, V> &m, bool show_info,
                     std::ostream &out, SourceLocation loc,
                     int indent_width = 4) {
  maybe_print_info(m, show_info, out, loc);
  out << "{\n";
  bool first = true;
  for (const auto &kv : sorted_map_items(m)) {
    format_map_entry(kv.first, *kv.second, first, 1, out, loc, indent_width);
    first = false;
  }
  out << "\n}" << std::endl;
}
#else
template <typename V, typename SourceLocation>
void format_map_value(const V &value, int indent_level, std::ostream &out,
                      SourceLocation loc, int indent_width, std::true_type) {
  out << "{\n";
  bool nested_first = true;
  for (const auto &kv : value) {
    format_map_entry(kv.first, kv.second, nested_first, indent_level + 1, out,
                     loc, indent_width);
    nested_first = false;
  }
  out << "\n" << std::string(indent_level * indent_width, ' ') << "}";
}

template <typename V, typename SourceLocation>
void format_map_value(const V &value, int indent_level, std::ostream &out,
                      SourceLocation, int indent_width, std::false_type) {
  (void)indent_level;
  (void)indent_width;
  out << "\"" << value << "\"";
}

template <typename K, typename V, typename SourceLocation>
void format_map_entry(const K &key, const V &value, bool is_first_element,
                      int indent_level, std::ostream &out, SourceLocation loc,
                      int indent_width = 4) {
  if (!is_first_element) {
    out << ",\n";
  }
  out << std::string(indent_level * indent_width, ' ') << "\"" << key << "\": ";
  format_map_value(value, indent_level, out, loc, indent_width,
                   typename is_map_like<V>::type{});
}

template <typename K, typename V, typename SourceLocation>
void format_map_like(const std::map<K, V> &m, bool show_info, std::ostream &out,
                     SourceLocation loc, int indent_width = 4) {
  maybe_print_info(m, show_info, out, loc);
  out << "{\n";
  bool first = true;
  for (const auto &kv : m) {
    format_map_entry(kv.first, kv.second, first, 1, out, loc, indent_width);
    first = false;
  }
  out << "\n}" << std::endl;
}

template <typename K, typename V, typename SourceLocation>
void format_map_like(const std::unordered_map<K, V> &m, bool show_info,
                     std::ostream &out, SourceLocation loc,
                     int indent_width = 4) {
  maybe_print_info(m, show_info, out, loc);
  out << "{\n";
  std::vector<std::pair<K, const V *>> items;
  items.reserve(m.size());
  for (const auto &kv : m)
    items.emplace_back(kv.first, &kv.second);
  std::sort(items.begin(), items.end(),
            [](const std::pair<K, const V *> &a,
               const std::pair<K, const V *> &b) { return a.first < b.first; });
  bool first = true;
  for (const auto &kv : items) {
    format_map_entry(kv.first, *kv.second, first, 1, out, loc, indent_width);
    first = false;
  }
  out << "\n}" << std::endl;
}
#endif

// Format containers with optional source location
#if M1104_HAS_CXX20
template <typename Container>
std::string
format_container(const Container &c, bool show_info = true,
                 std::source_location loc = std::source_location::current()) {
  std::ostringstream out;
  if constexpr (is_map_like<Container>::value) {
    format_map_like(c, show_info, out, loc);
  } else if constexpr (is_sequence<Container>::value) {
    format_sequence(c, show_info, out, loc);
  } else {
    format_sequence(c, show_info, out, loc);
  }
  return out.str();
}
#elif M1104_HAS_CXX17
template <typename Container>
std::string format_container(const Container &c, bool show_info = true,
                             no_source_location loc = no_source_location()) {
  std::ostringstream out;
  if constexpr (is_map_like<Container>::value) {
    format_map_like(c, show_info, out, loc);
  } else if constexpr (is_sequence<Container>::value) {
    format_sequence(c, show_info, out, loc);
  } else {
    format_sequence(c, show_info, out, loc);
  }
  return out.str();
}
#else
// C++11/14 替代：用标签分派避免 if constexpr，同时在注释说明决策流程。
// format_container_select 根据 is_map_like / is_sequence 的
// true_type/false_type 重载选择，避免实例化不适用的分支。
template <typename Container, typename SourceLocation>
void format_container_select(const Container &c, bool show_info,
                             std::ostream &out, SourceLocation loc,
                             int indent_width, std::true_type,
                             std::false_type) {
  format_map_like(c, show_info, out, loc, indent_width);
}

template <typename Container, typename SourceLocation>
void format_container_select(const Container &c, bool show_info,
                             std::ostream &out, SourceLocation loc,
                             int /*indent_width*/, std::false_type,
                             std::true_type) {
  format_sequence(c, show_info, out, loc);
}

template <typename Container, typename SourceLocation>
void format_container_select(const Container &c, bool show_info,
                             std::ostream &out, SourceLocation loc,
                             int /*indent_width*/, std::false_type,
                             std::false_type) {
  format_sequence(c, show_info, out, loc);
}

template <typename Container>
std::string format_container(const Container &c, bool show_info = true,
                             no_source_location loc = no_source_location()) {
  std::ostringstream out;
  format_container_select(c, show_info, out, loc, 4,
                          typename is_map_like<Container>::type{},
                          typename is_sequence<Container>::type{});
  return out.str();
}
#endif

// Traits
template <typename T> struct is_supported : std::false_type {};
template <typename T> struct is_supported<std::vector<T>> : std::true_type {};
template <typename T> struct is_supported<std::list<T>> : std::true_type {};
template <typename T>
struct is_supported<std::forward_list<T>> : std::true_type {};
template <typename K, typename V>
struct is_supported<std::map<K, V>> : std::true_type {};
template <typename K, typename V>
struct is_supported<std::unordered_map<K, V>> : std::true_type {};

// Supported formatting dispatcher
#if M1104_HAS_CXX17
template <typename T> std::string format_supported(const T &val) {
  // 如果是已支持的容器（序列或 map_like），走结构化格式化；否则退回流式输出。
  if constexpr (is_sequence<T>::value || is_map_like<T>::value) {
    return format_container(val);
  } else {
    // 若类型未提供可访问的 operator<<，此处会在编译期报错（无法插入到流中）。
    std::ostringstream oss;
    oss << val;
    return oss.str();
  }
}

template <typename T> std::string format_value(const T &val) {
  // 先用 is_supported 筛容器，容器走 format_supported，其余直接流式；区别在于
  // format_value 覆盖所有可用类型。 std::decay_t<T> 去除引用/const/volatile
  // 等修饰，把类型归一化再查 is_supported，避免同一类型因 cvref
  // 差异导致重复判定。
  if constexpr (is_supported<std::decay_t<T>>::value) {
    return format_supported(val);
  } else {
    std::ostringstream oss;
    oss << val;
    return oss.str();
  }
}
#else
// C++11/14 替代：用 enable_if 选择支持的容器分支，避免 if constexpr。
// is_sequence/is_map_like 结果作为编译期常量控制重载，防止不适用分支被实例化。
template <typename T>
typename std::enable_if<is_sequence<T>::value || is_map_like<T>::value,
                        std::string>::type
format_supported(const T &val) {
  // 支持的容器走格式化
  return format_container(val);
}

template <typename T>
typename std::enable_if<!is_sequence<T>::value && !is_map_like<T>::value,
                        std::string>::type
format_supported(const T &val) {
  // 非容器退回流式输出
  // 若类型未提供可访问的 operator<<，此处会在编译期报错（无法插入到流中）。
  std::ostringstream oss;
  oss << val;
  return oss.str();
}

template <typename T>
typename std::enable_if<is_supported<typename std::decay<T>::type>::value,
                        std::string>::type
format_value(const T &val) {
  // 支持的容器走 format_supported
  return format_supported(val);
}

template <typename T>
typename std::enable_if<!is_supported<typename std::decay<T>::type>::value,
                        std::string>::type
format_value(const T &val) {
  // 非支持容器退回流式输出
  // 若类型未提供可访问的 operator<<，此处会在编译期报错（无法插入到流中）。
  std::ostringstream oss;
  oss << val;
  return oss.str();
}
#endif

} // namespace log_detail
} // namespace mental1104

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
// 前置声明：global_level 的初始化 lambda 需调用
// to_spd_level，因此先声明再在下方定义。
inline spdlog::level::level_enum to_spd_level(LogLevel lvl);
#endif

inline LogLevel level_from_string(string_view text) {
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

inline LogLevel default_level() {
  return level_from_string(M1104_LOG_DEFAULT_LEVEL);
}

inline LogLevel env_level() {
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

// global_level 返回进程级日志等级的原子引用：首次调用用 env_level()
// 初始化，且在启用 spdlog 时借助静态 lambda 把 spdlog
// 的全局等级同步到相同值（只执行一次）。 如果把整个 global_level 函数改成
// static（内部链接），每个翻译单元会有自己独立的静态
// lvl/init_spdlog，日志等级就无法在全程序共享；这里保持外部链接的
// inline，确保所有使用方看到同一份原子等级。 用 std::atomic
// 是为了并发安全地读/改日志等级：多线程可能同时调用
// set_log_level/get_log_level，原子避免数据竞争未定义行为；这里的使用只需值一致，不需要顺序同步，所以读写都用
// relaxed。
inline std::atomic<LogLevel> &global_level() {
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

// level_rank 仅将枚举转为 int 用于比较排序阈值；不能直接 static_cast 成
// std::string（那不是合法转换），若要字符串请用 level_name。
inline int level_rank(LogLevel lvl) { return static_cast<int>(lvl); }

#if M1104_HAS_CXX17
// to_string 把可变参序列按顺序写入 ostringstream 再取出字符串：支持类型走
// format_value，其余直接用 operator<<；最后用折叠表达式 (append(...), ...)
// 追加所有参数。
template <typename... Args> std::string to_string(Args &&...args) {
  std::ostringstream oss;
  // 这里的 append 闭包按值类别转发每个参数：捕获 oss 引用（& 只抓住外层的
  // oss），形参 auto&& 是转发引用，既接受左值也接受右值；auto&
  // 无法接收右值，auto 会复制/丢掉引用语义。
  auto append = [&](auto &&val) {
    using Decayed =
        std::decay_t<decltype(val)>; // Decayed 去掉引用/cv 修饰，统一类型后判
                                     // is_supported。
    if constexpr (mental1104::log_detail::is_supported<Decayed>::value) {
      // format_value 对 is_supported
      // 的容器（vector/list/forward_list/map/unordered_map）走结构化输出，其余类型退回
      // operator<<；因此不是只限 STL 容器，任何可流式输出的类型都会写入。
      oss << mental1104::log_detail::format_value(val);
    } else {
      // 非受支持容器且没有 operator<<
      // 会在编译期报错（类型无法插入到流）；这是期望行为，便于调用点暴露缺少的格式化支持。
      oss << std::forward<decltype(val)>(
          val); // forward 还原值类别，右值参数保持右值传给 operator<</move-only
                // 类型。
    }
  };
  (append(std::forward<Args>(args)),
   ...); // 折叠表达式：展开实参包，依次调用 append。
  return oss.str();
}
#else
// C++11/14 替代：用 enable_if 辅助函数 + 初始化列表展开代替折叠表达式/if
// constexpr。 append_one 通过 is_supported 在编译期分发，dummy
// 数组展开模拟折叠表达式。
template <typename T>
typename std::enable_if<mental1104::log_detail::is_supported<
    typename std::decay<T>::type>::value>::type
append_one(std::ostringstream &oss, T &&val) {
  oss << mental1104::log_detail::format_value(std::forward<T>(val));
}

template <typename T>
typename std::enable_if<!mental1104::log_detail::is_supported<
    typename std::decay<T>::type>::value>::type
append_one(std::ostringstream &oss, T &&val) {
  oss << std::forward<T>(val);
}

template <typename... Args> std::string to_string(Args &&...args) {
  std::ostringstream oss;
  int dummy[] = {0, (append_one(oss, std::forward<Args>(args)), 0)...};
  (void)dummy;
  return oss.str();
}
#endif

#if M1104_HAS_SPDLOG
inline spdlog::level::level_enum to_spd_level(LogLevel lvl) {
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

template <typename T> struct is_printf_arg {
  using Decayed = typename std::decay<T>::type;
  static constexpr bool value =
      (std::is_arithmetic<Decayed>::value ||
       std::is_enum<Decayed>::value || std::is_pointer<Decayed>::value) &&
      !std::is_same<Decayed, std::string>::value &&
      !std::is_same<Decayed, string_view>::value;
};

template <typename... Args> struct are_printf_args_safe;

template <> struct are_printf_args_safe<> : std::true_type {};

template <typename T, typename... Rest>
struct are_printf_args_safe<T, Rest...>
    : std::integral_constant<bool, is_printf_arg<T>::value &&
                                       are_printf_args_safe<Rest...>::value> {};

template <typename... Args>
std::string format_printf_impl(std::true_type, string_view fmt,
                               Args &&...args) {
  std::string fmt_cstr(fmt);
  int size =
      std::snprintf(nullptr, 0, fmt_cstr.c_str(), std::forward<Args>(args)...);
  if (size <= 0) {
    return to_string(fmt, " ", std::forward<Args>(args)...);
  }
  std::string buf(static_cast<size_t>(size), '\0');
  std::snprintf(buf.data(), static_cast<size_t>(size) + 1, fmt_cstr.c_str(),
                std::forward<Args>(args)...);
  return buf;
}

template <typename... Args>
std::string format_printf_impl(std::false_type, string_view fmt,
                               Args &&...args) {
  return to_string(fmt, " ", std::forward<Args>(args)...);
}

template <typename... Args>
// format_printf：用 snprintf 风格格式化。先把 string_view 复制成以 '\0' 结尾的
// std::string，第一次 snprintf(nullptr,0,...) 计算所需长度，失败则退回
// to_string 直接拼接；成功则按长度分配缓冲，第二次 snprintf 写入（长度+1
// 覆盖终止符），最终返回缓冲内容 （保留 size 个有效字符，终止符被丢弃在 string
// 末尾）。
std::string format_printf(string_view fmt, Args &&...args) {
  return format_printf_impl(are_printf_args_safe<Args...>{}, fmt,
                            std::forward<Args>(args)...);
}

inline bool has_brace_format(string_view fmt) {
  // string_view 不论是 std::string_view 还是回退 std::string，都提供 npos
  // 哨兵；用于 find 未命中判定。
  return fmt.find('{') != string_view::npos;
}

inline bool has_printf_format(string_view fmt) {
  return fmt.find('%') != string_view::npos;
}

template <typename... Args>
// format_flexible：根据格式串内容/能力选择格式方案——优先检测花括号占位且有
// <format> 时走 std::vformat； 否则若含 '%' 走 printf 风格；若两者都不匹配且有
// <format> 则仍用 vformat 直接格式化；都不可用时退回 to_string 拼接。
// 若格式串同时包含 '{' 与 '%'：有 <format> 时走 vformat，'%' 只是普通字符；没有
// <format> 时若含 '%' 则走 printf，'{' 按普通字符处理，无法同时混用两套占位符。
std::string format_flexible(string_view fmt, Args &&...args) {
#if M1104_HAS_STD_FORMAT
  if (has_brace_format(fmt)) {
    return std::vformat(fmt,
                        std::make_format_args(std::forward<Args>(args)...));
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
  // 读取当前全局日志等级（relaxed 原子读取）。
  return detail::global_level().load(std::memory_order_relaxed);
}

inline void set_log_level(LogLevel lvl) {
  // 更新全局等级；若启用 spdlog 同步其全局 level。
  detail::global_level().store(lvl, std::memory_order_relaxed);
#if M1104_HAS_SPDLOG
  spdlog::set_level(detail::to_spd_level(lvl));
#endif
}

template <typename... Args> inline void log(LogLevel level, Args &&...args) {
  // 过滤低于当前全局等级的日志。
  if (detail::level_rank(level) < detail::level_rank(get_log_level())) {
    return;
  }
  // 构造消息后输出到 spdlog 或 stdout/stderr。
  auto msg = detail::to_string(std::forward<Args>(args)...);
#if M1104_HAS_SPDLOG
  spdlog::log(detail::to_spd_level(level), msg);
#else
  auto &out = (level == LogLevel::Error) ? std::cerr : std::cout;
  out << "[" << detail::level_name(level) << "] " << msg << std::endl;
#endif
}

template <typename... Args>
inline void logf(LogLevel level, string_view fmt, Args &&...args) {
  // 同样先做等级过滤。
  if (detail::level_rank(level) < detail::level_rank(get_log_level())) {
    return;
  }
  // 根据格式串自动选择 std::format/printf/拼接，再输出。
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

// 便捷宏：格式化版本（printf/std::format/拼接自动适配）。
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
