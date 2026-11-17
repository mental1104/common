#ifndef MENTAL1104_LOG_H
#define MENTAL1104_LOG_H

/// @file
/// 分级日志入口和消息格式化；容器布局由日志内部模板实现，运行时状态由库持有。
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "mental1104/logging/detail/container_printer.h"
#include "mental1104/meta/format_support.h"
#include "mental1104/util.h"

// 使用字符串路径探测，避免 formatter 在尖括号内插入空格后误判依赖缺失。
#if M1104_HAS_INCLUDE("spdlog/spdlog.h")
#ifndef SPDLOG_HEADER_ONLY
#define SPDLOG_HEADER_ONLY
#endif
#include <spdlog/spdlog.h>
#define M1104_HAS_SPDLOG 1
#else
#define M1104_HAS_SPDLOG 0
#endif

#ifndef M1104_LOG_DEFAULT_LEVEL
#define M1104_LOG_DEFAULT_LEVEL "info"
#endif

namespace mental1104 {

/// 日志过滤阈值；枚举值越低，允许输出的消息越详细。
enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

namespace detail {

/// 返回日志等级的静态名称。
/// @param lvl 日志等级。
/// @return 无需释放的静态字符串，未知等级为 UNKNOWN。
const char *level_name(LogLevel lvl);

/// 解析不区分大小写的日志等级名称。
/// @param text 只读等级文本，不保存视图。
/// @return 解析结果；未知名称返回 Info。
LogLevel level_from_string(string_view text);

/// 读取构建日志库时指定的默认日志等级。
/// @return 编译配置对应的等级。
LogLevel default_level();

/// 读取 MENTAL1104_LOG_LEVEL，缺失或为空时使用库的默认等级。
/// @return 初始日志等级。
LogLevel env_level();

/// 取得由日志库统一初始化的原子等级，并同步可选 spdlog 后端。
/// @return 库持有的等级引用，调用方不负责释放。
std::atomic<LogLevel> &global_level();

/// 输出已经完成类型适配和格式化的消息，不再次过滤等级。
/// @param level 消息等级；Error 的回退输出使用 stderr，其余使用 stdout。
/// @param message 当前调用期间有效的只读消息，不保存引用。
void write_log_message(LogLevel level, const std::string &message);

/// 将日志等级转换为过滤时比较的次序。
/// @param lvl 待比较的日志等级。
/// @return 与枚举顺序一致的整数。
inline int level_rank(LogLevel lvl) { return static_cast<int>(lvl); }

/// 将已注册类型按结构化布局写入消息流，不取得输入所有权。
/// @param out 接收消息片段的流。
/// @param val 本次调用期间有效的输入值，保留其值类别。
template <typename T>
typename std::enable_if<
    log_detail::is_supported<typename std::decay<T>::type>::value>::type
append_one(std::ostringstream &out, T &&val) {
    out << log_detail::format_value(std::forward<T>(val));
}

/// 普通类型直接流式输出，保留仅接受右值的 operator<< 支持。
/// @param out 接收消息片段的流。
/// @param val 本次调用期间有效的输入值，保留其值类别。
template <typename T>
typename std::enable_if<
    !log_detail::is_supported<typename std::decay<T>::type>::value>::type
append_one(std::ostringstream &out, T &&val) {
    out << std::forward<T>(val);
}

/// 按从左到右的顺序拼接消息片段，所有标准复用同一实现。
/// @param args 本次调用期间有效的输入片段；不保存引用。
/// @return 独立持有的消息文本；空参数包产生空串。
template <typename... Args>
std::string to_string(Args &&...args) {
    std::ostringstream out;
    // 初始化列表保证求值顺序，同时允许 C++11 和空参数包。
    const int appended[] = {
        0, (append_one(out, std::forward<Args>(args)), 0)...};
    (void)appended;
    return out.str();
}

#if M1104_HAS_SPDLOG
/// 将公共日志等级映射到编译时选定的 spdlog 后端。
/// @param lvl 公共等级。
/// @return 对应 spdlog 等级。
spdlog::level::level_enum to_spd_level(LogLevel lvl);
#endif

/// 判断参数能否沿用现有 C printf 可变参数入口，不接受 std::string/view 对象。
template <typename T>
struct is_printf_arg {
    using Decayed = typename std::decay<T>::type;
    static constexpr bool value =
        (std::is_arithmetic<Decayed>::value || std::is_enum<Decayed>::value ||
            std::is_pointer<Decayed>::value) &&
        !std::is_same<Decayed, std::string>::value &&
        !std::is_same<Decayed, string_view>::value;
};

/// 递归检查全部参数是否可交给 printf；不验证格式串与类型是否匹配。
template <typename... Args>
struct are_printf_args_safe;
/// 空参数包不包含不安全类型。
template <>
struct are_printf_args_safe<> : std::true_type {};
/// 只有当前参数和其余参数都通过时才启用 printf。
template <typename T, typename... Rest>
struct are_printf_args_safe<T, Rest...>
    : std::integral_constant<bool,
          is_printf_arg<T>::value && are_printf_args_safe<Rest...>::value> {};

/// 由已编译核心执行 printf 的两轮缓冲区处理，不保存参数引用。
/// @param format NUL 结尾的格式串，本次调用期间有效。
/// @param ... 按 C 可变参数规则提升、且与格式串匹配的参数。
/// @return 独立结果；长度计算失败或结果为空时返回空串，由模板入口选择回退。
std::string format_printf_message(const char *format, ...);

/// 使用 snprintf 生成格式化结果；长度计算失败时保留原有拼接回退。
/// @param safe 表示参数类型可传入 printf 的编译期标记。
/// @param fmt 当前调用借用的格式串，复制后保证 NUL 结尾。
/// @param args 必须与格式串的 printf 占位符匹配的参数。
/// @return 独立持有的结果；格式串与参数不匹配仍属于调用方错误。
template <typename... Args>
std::string format_printf_impl(
    std::true_type /*safe*/, string_view fmt, Args &&...args) {
    const std::string format(fmt);
    const auto message =
        format_printf_message(format.c_str(), std::forward<Args>(args)...);
    if (message.empty())
        return to_string(fmt, " ", std::forward<Args>(args)...);
    return message;
}

/// 不安全的参数类型不进入 C 可变参数函数，沿用流式拼接回退。
/// @param safe 表示至少一个参数不适合 printf 的编译期标记。
/// @param fmt 当前调用借用的格式串。
/// @param args 本次调用期间有效的消息参数。
/// @return 独立持有的格式串与参数拼接文本。
template <typename... Args>
std::string format_printf_impl(
    std::false_type /*safe*/, string_view fmt, Args &&...args) {
    return to_string(fmt, " ", std::forward<Args>(args)...);
}

/// 按参数的静态类型选择 printf 或安全的拼接回退。
/// @param fmt 当前调用借用的格式串。
/// @param args 与 printf 占位符匹配的参数，或需要回退的对象。
/// @return 独立持有的格式化结果。
template <typename... Args>
std::string format_printf(string_view fmt, Args &&...args) {
    return format_printf_impl(
        are_printf_args_safe<Args...>{}, fmt, std::forward<Args>(args)...);
}

/// 检查格式串中是否存在花括号格式的起始标记。
/// @param fmt 当前调用借用的格式串。
/// @return 含左花括号时为 true，不负责验证完整格式语法。
inline bool has_brace_format(string_view fmt) {
    return fmt.find('{') != string_view::npos;
}

/// 检查格式串中是否存在 printf 的起始标记。
/// @param fmt 当前调用借用的格式串。
/// @return 含百分号时为 true，不负责验证完整格式语法。
inline bool has_printf_format(string_view fmt) {
    return fmt.find('%') != string_view::npos;
}

// 只在能力边界选择完整实现；外层格式选择流程不夹杂条件编译。
#if M1104_HAS_STD_FORMAT
/// 通过标准格式化库处理花括号文本，格式错误继续向调用方抛出。
/// @param fmt 当前调用借用的标准格式串。
/// @param args 本次调用期间有效的标准格式化参数。
/// @return 独立持有的格式化结果。
template <typename... Args>
std::string format_text(string_view fmt, Args &&...args) {
    // make_format_args 借用具名参数；不得转发为短命右值或保存参数存储。
    return std::vformat(fmt, std::make_format_args(args...));
}
#else
/// 无标准格式化能力时保留原有拼接规则，不伪装成花括号格式化。
/// @param fmt 当前调用借用的格式串。
/// @param args 本次调用期间有效的消息参数。
/// @return 格式串、一个空格和各参数拼接而成的独立文本。
template <typename... Args>
std::string format_text(string_view fmt, Args &&...args) {
    return to_string(fmt, " ", std::forward<Args>(args)...);
}
#endif

/// 按现有优先级选择格式方案：标准花括号、printf、标准文本或拼接回退。
/// @param fmt 当前调用借用的格式串；混合占位符时不尝试同时解释两套语法。
/// @param args 本次调用期间有效、与所选格式方案匹配的参数。
/// @return 独立持有的结果；标准格式串无效时继续抛出 format_error。
template <typename... Args>
std::string format_flexible(string_view fmt, Args &&...args) {
    const bool prefer_braces = M1104_HAS_STD_FORMAT && has_brace_format(fmt);
    if (!prefer_braces && has_printf_format(fmt)) {
        return format_printf(fmt, std::forward<Args>(args)...);
    }
    return format_text(fmt, std::forward<Args>(args)...);
}

}  // namespace detail

/// 读取日志库当前等级。
/// @return 当前过滤阈值。
LogLevel get_log_level();

/// 更新日志库等级并同步可选后端。
/// @param lvl 新的过滤阈值。
void set_log_level(LogLevel lvl);

/// 按等级提供拼接和格式化方法的无状态入口；过滤阈值与后端由已编译核心共享。
/// 不持有资源或独立等级，不需要调用方创建、关闭或释放实例。
class Logger final {
public:
    /// 创建无状态入口，不初始化后端或改变共享日志等级。
    constexpr Logger() noexcept = default;

    /// 按 Debug 等级拼接消息，原样保留文本中的占位符。
    /// @param args 本次调用期间有效的消息片段，保留其值类别，不保存引用。
    template <typename... Args>
    void debug(Args &&...args) const {
        this->write(LogLevel::Debug, std::forward<Args>(args)...);
    }

    /// 按 Info 等级拼接消息，原样保留文本中的占位符。
    /// @param args 本次调用期间有效的消息片段，保留其值类别，不保存引用。
    template <typename... Args>
    void info(Args &&...args) const {
        this->write(LogLevel::Info, std::forward<Args>(args)...);
    }

    /// 按 Warning 等级拼接消息，原样保留文本中的占位符。
    /// @param args 本次调用期间有效的消息片段，保留其值类别，不保存引用。
    template <typename... Args>
    void warning(Args &&...args) const {
        this->write(LogLevel::Warning, std::forward<Args>(args)...);
    }

    /// 按 Error 等级拼接消息，原样保留文本中的占位符。
    /// @param args 本次调用期间有效的消息片段，保留其值类别，不保存引用。
    template <typename... Args>
    void error(Args &&...args) const {
        this->write(LogLevel::Error, std::forward<Args>(args)...);
    }

    /// 按 Debug 等级格式化消息；过滤时不解释格式串或构造消息。
    /// @param fmt 当前调用借用的格式串。
    /// @param args 与所选格式方案匹配的参数，不保存引用。
    template <typename... Args>
    void debugf(string_view fmt, Args &&...args) const {
        this->write_formatted(
            LogLevel::Debug, fmt, std::forward<Args>(args)...);
    }

    /// 按 Info 等级格式化消息；标准格式错误继续抛出 format_error。
    /// @param fmt 当前调用借用的格式串。
    /// @param args 与所选格式方案匹配的参数，不保存引用。
    template <typename... Args>
    void infof(string_view fmt, Args &&...args) const {
        this->write_formatted(LogLevel::Info, fmt, std::forward<Args>(args)...);
    }

    /// 按 Warning 等级格式化消息；标准格式错误继续抛出 format_error。
    /// @param fmt 当前调用借用的格式串。
    /// @param args 与所选格式方案匹配的参数，不保存引用。
    template <typename... Args>
    void warningf(string_view fmt, Args &&...args) const {
        this->write_formatted(
            LogLevel::Warning, fmt, std::forward<Args>(args)...);
    }

    /// 按 Error 等级格式化消息；标准格式错误继续抛出 format_error。
    /// @param fmt 当前调用借用的格式串。
    /// @param args 与所选格式方案匹配的参数，不保存引用。
    template <typename... Args>
    void errorf(string_view fmt, Args &&...args) const {
        this->write_formatted(
            LogLevel::Error, fmt, std::forward<Args>(args)...);
    }

private:
    /// 在拼接前统一过滤等级，保留任意调用方流式类型。
    /// @param level 当前方法对应的日志等级。
    /// @param args 本次调用期间有效的消息片段，不保存引用。
    template <typename... Args>
    void write(LogLevel level, Args &&...args) const {
        if (detail::level_rank(level) < detail::level_rank(get_log_level()))
            return;
        const auto message = detail::to_string(std::forward<Args>(args)...);
        detail::write_log_message(level, message);
    }

    /// 在格式化前统一过滤等级，不吞掉已启用消息的格式错误。
    /// @param level 当前方法对应的日志等级。
    /// @param fmt 当前调用借用的格式串。
    /// @param args 与所选格式方案匹配的参数，不保存引用。
    template <typename... Args>
    void write_formatted(
        LogLevel level, const string_view &fmt, Args &&...args) const {
        if (detail::level_rank(level) < detail::level_rank(get_log_level()))
            return;
        const auto message =
            detail::format_flexible(fmt, std::forward<Args>(args)...);
        detail::write_log_message(level, message);
    }
};

/// 公开的点调用入口；C++11
/// 起各翻译单元拥有无状态常量，共享库仅持有一份可变状态。
/// 不导出全局数据符号，也不依赖 C++17 inline 变量或跨翻译单元的构造顺序。
constexpr Logger log{};

#define M1104_LOG_DEBUG(...) ::mental1104::log.debug(__VA_ARGS__)
#define M1104_LOG_INFO(...) ::mental1104::log.info(__VA_ARGS__)
#define M1104_LOG_WARNING(...) ::mental1104::log.warning(__VA_ARGS__)
#define M1104_LOG_ERROR(...) ::mental1104::log.error(__VA_ARGS__)

// 格式串作为首个实参整体转发，零格式参数也不需要 GNU 逗号吞并扩展。
#define M1104_LOG_DEBUGF(...) ::mental1104::log.debugf(__VA_ARGS__)
#define M1104_LOG_INFOF(...) ::mental1104::log.infof(__VA_ARGS__)
#define M1104_LOG_WARNINGF(...) ::mental1104::log.warningf(__VA_ARGS__)
#define M1104_LOG_ERRORF(...) ::mental1104::log.errorf(__VA_ARGS__)

}  // namespace mental1104

#endif  // MENTAL1104_LOG_H
