#ifndef MENTAL1104_LOGGING_DETAIL_CONTAINER_PRINTER_H
#define MENTAL1104_LOGGING_DETAIL_CONTAINER_PRINTER_H

/// @file
/// 日志内部的容器打印实现；模板保留调用方类型，版本差异只影响必要的适配点。
#include <algorithm>
#include <forward_list>
#include <iterator>
#include <list>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX20 && M1104_HAS_INCLUDE(<source_location>)
#include <source_location>
#endif
#if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201907L
#define M1104_LOG_HAS_SOURCE_LOCATION 1
#else
#define M1104_LOG_HAS_SOURCE_LOCATION 0
/// 不支持 source_location 时保留既有参数类型，不伪造调用位置。
struct no_source_location {};
#endif

namespace mental1104 {
namespace log_detail {

// 既有输出契约随标准存在的两处差异集中在此，打印主流程只使用语义规则。
using sorted_map_layout = std::integral_constant<bool, M1104_HAS_CXX17>;
constexpr bool count_unsized_containers = !M1104_HAS_CXX17;

// 默认实参必须留在调用方可见的声明中，不能通过普通包装函数取得调用位置。
#if M1104_LOG_HAS_SOURCE_LOCATION
using source_location_type = std::source_location;
template <typename Container>
std::string format_container(const Container &c, bool show_info = true,
    source_location_type loc = std::source_location::current());

/// 输出真实调用位置。
/// @param loc 当前调用期间有效的位置快照。
/// @param out 接收前缀的输出流，不取得所有权。
template <typename SourceLocation>
void print_source_location(SourceLocation loc, std::ostream &out) {
    out << "[File: " << loc.file_name() << ", Line: " << loc.line() << "] ";
}
#else
using source_location_type = no_source_location;
template <typename Container>
std::string format_container(const Container &c, bool show_info = true,
    source_location_type loc = no_source_location());

/// 没有调用位置能力时不输出位置，不为主流程引入版本判断。
/// @param loc 兼容占位值，不保存。
/// @param out 保持不变的输出流。
template <typename SourceLocation>
void print_source_location(SourceLocation /*loc*/, std::ostream & /*out*/) {}
#endif

/// 为 C++11 起的表达式探测提供 void 类型，不依赖 std::void_t。
template <typename...>
struct make_void {
    using type = void;
};

/// 检查只读容器是否提供 size()；不存在时保留编译期 false。
template <typename T, typename = void>
struct has_size_method : std::false_type {};
/// size() 表达式有效时启用大小读取。
template <typename T>
struct has_size_method<T,
    typename make_void<decltype(std::declval<const T &>().size())>::type>
    : std::true_type {};

/// 标记日志已支持的序列类型，其他类型仍沿用普通流式输出。
template <typename T>
struct is_sequence : std::false_type {};
template <typename T>
struct is_sequence<std::vector<T>> : std::true_type {};
template <typename T>
struct is_sequence<std::list<T>> : std::true_type {};
template <typename T>
struct is_sequence<std::forward_list<T>> : std::true_type {};

/// 标记使用键值布局打印的 map 类型。
template <typename T>
struct is_map_like : std::false_type {};
template <typename K, typename V>
struct is_map_like<std::map<K, V>> : std::true_type {};
template <typename K, typename V>
struct is_map_like<std::unordered_map<K, V>> : std::true_type {};

/// 读取无 size() 容器的元素个数，不改变容器。
/// @param c 当前调用期间有效的只读容器。
/// @return begin/end 之间的元素数。
template <typename Container>
std::size_t distance_size(const Container &c) {
    return static_cast<std::size_t>(std::distance(c.begin(), c.end()));
}

/// 优先使用容器自带的 size()。
/// @param c 当前调用期间有效的只读容器。
/// @param out 接收大小前缀的流。
/// @param priority int 重载优先参与选择，不具有运行时含义。
template <typename Container>
auto print_size_fallback(const Container &c, std::ostream &out,
    int /*priority*/) -> decltype(c.size(), void()) {
    out << "(size: " << c.size() << ") " << std::endl;
}

/// 保留旧标准对无 size() 容器的计数行为；C++17 起仍省略该信息。
/// @param c 当前调用期间有效的只读容器。
/// @param out 接收大小前缀的流。
/// @param priority size() 不可用时选中的低优先级重载。
template <typename Container>
void print_size_fallback(
    const Container &c, std::ostream &out, long /*priority*/) {
    if (count_unsized_containers) {
        out << "(size: " << distance_size(c) << ") " << std::endl;
    }
}

/// 按容器能力选择大小读取方式，不要求调用方理解重载优先级。
/// @param c 当前调用期间有效的只读容器。
/// @param out 接收大小前缀的流。
template <typename Container>
void print_size_fallback(const Container &c, std::ostream &out) {
    print_size_fallback(c, out, 0);
}

/// 按需依次输出调用位置和容器大小；隐藏信息时不遍历容器。
/// @param c 当前调用期间有效的只读容器。
/// @param show_info 是否输出元信息。
/// @param out 接收元信息的流。
/// @param loc 从公开入口传入的调用位置或兼容占位值。
template <typename Container, typename SourceLocation>
void maybe_print_info(
    const Container &c, bool show_info, std::ostream &out, SourceLocation loc) {
    if (!show_info) return;
    print_source_location(loc, out);
    print_size_fallback(c, out);
}

/// 各标准以同一份流式实现打印序列，不为相同布局引入 fmt/ranges 依赖。
/// @param c 当前调用期间有效的只读序列。
/// @param show_info 是否输出元信息。
/// @param out 接收完整序列文本的流。
/// @param loc 由调用方捕获的位置或占位值。
template <typename Container, typename SourceLocation>
void format_sequence(
    const Container &c, bool show_info, std::ostream &out, SourceLocation loc) {
    maybe_print_info(c, show_info, out, loc);
    out << "{";
    bool first = true;
    for (const auto &element : c) {
        if (!first) out << ", ";
        out << element;
        first = false;
    }
    out << "}" << std::endl;
}

/// 创建按键升序的条目快照；值通过指针借用原 map，不延长生命周期。
/// @param m 本次打印期间不得修改或销毁的只读 map。
/// @return 拥有键副本、借用原值的条目数组。
template <typename Map>
std::vector<
    std::pair<typename Map::key_type, const typename Map::mapped_type *>>
sorted_map_items(const Map &m) {
    using Item =
        std::pair<typename Map::key_type, const typename Map::mapped_type *>;
    std::vector<Item> items;
    items.reserve(m.size());
    for (const auto &kv : m) items.emplace_back(kv.first, &kv.second);
    // 使用明确的元素类型，C++11 调用方也可编译同一份排序实现。
    std::sort(items.begin(), items.end(),
        [](const Item &a, const Item &b) { return a.first < b.first; });
    return items;
}

/// 输出单个键值条目，嵌套 map 由静态类型分派递归打印。
/// @param key 本次调用借用的键，不保存引用。
/// @param value 本次调用借用的值，不修改。
/// @param is_first_element 是否省略前导逗号。
/// @param indent_level 当前缩进层数。
/// @param out 接收条目文本的流。
/// @param loc 透传的调用位置或占位值。
/// @param indent_width 每层缩进的空格数。
template <typename K, typename V, typename SourceLocation>
void format_map_entry(const K &key, const V &value, bool is_first_element,
    int indent_level, std::ostream &out, SourceLocation loc,
    int indent_width = 4);

/// 按键升序输出条目，供顶层 unordered_map 及新版嵌套布局复用。
/// @param m 本次打印期间保持存活且不修改的 map。
/// @param indent_level 当前条目缩进层数。
/// @param out 接收条目文本的流。
/// @param loc 透传的调用位置或占位值。
/// @param indent_width 每层缩进的空格数。
/// @param sorted 选择排序布局的编译期标记。
template <typename Map, typename SourceLocation>
void write_map_entries(const Map &m, int indent_level, std::ostream &out,
    SourceLocation loc, int indent_width, std::true_type /*sorted*/) {
    bool first = true;
    for (const auto &kv : sorted_map_items(m)) {
        format_map_entry(
            kv.first, *kv.second, first, indent_level, out, loc, indent_width);
        first = false;
    }
}

/// 按原迭代顺序输出条目，保留 C++11/14 的 map 和嵌套布局。
/// @param m 本次打印期间保持存活且不修改的 map。
/// @param indent_level 当前条目缩进层数。
/// @param out 接收条目文本的流。
/// @param loc 透传的调用位置或占位值。
/// @param indent_width 每层缩进的空格数。
/// @param sorted 选择原迭代顺序的编译期标记。
template <typename Map, typename SourceLocation>
void write_map_entries(const Map &m, int indent_level, std::ostream &out,
    SourceLocation loc, int indent_width, std::false_type /*sorted*/) {
    bool first = true;
    for (const auto &kv : m) {
        format_map_entry(
            kv.first, kv.second, first, indent_level, out, loc, indent_width);
        first = false;
    }
}

/// 打印嵌套 map，按既有标准差异选择条目顺序，不改变键值布局。
/// @param value 本次调用借用的嵌套 map。
/// @param indent_level 外层条目的缩进层数。
/// @param out 接收嵌套文本的流。
/// @param loc 透传的调用位置或占位值。
/// @param indent_width 每层缩进的空格数。
/// @param map_tag 表示值是 map 的编译期标记。
template <typename V, typename SourceLocation>
void format_map_value(const V &value, int indent_level, std::ostream &out,
    SourceLocation loc, int indent_width, std::true_type /*map_tag*/) {
    out << "{\n";
    write_map_entries(
        value, indent_level + 1, out, loc, indent_width, sorted_map_layout{});
    out << "\n" << std::string(indent_level * indent_width, ' ') << "}";
}

/// 普通 map 值沿用带引号的流式输出，不尝试递归打印非 map 类型。
/// @param value 本次调用借用的普通值。
/// @param indent_level 外层缩进层数，此分支不使用。
/// @param out 接收值文本的流。
/// @param loc 调用位置，此分支不使用。
/// @param indent_width 每层缩进宽度，此分支不使用。
/// @param map_tag 表示值不是 map 的编译期标记。
template <typename V, typename SourceLocation>
void format_map_value(const V &value, int /*indent_level*/, std::ostream &out,
    SourceLocation /*loc*/, int /*indent_width*/, std::false_type /*map_tag*/) {
    out << "\"" << value << "\"";
}

template <typename K, typename V, typename SourceLocation>
void format_map_entry(const K &key, const V &value, bool is_first_element,
    int indent_level, std::ostream &out, SourceLocation loc, int indent_width) {
    if (!is_first_element) out << ",\n";
    out << std::string(indent_level * indent_width, ' ') << "\"" << key
        << "\": ";
    format_map_value(value, indent_level, out, loc, indent_width,
        typename is_map_like<V>::type{});
}

/// 打印有序 map，保留旧标准迭代顺序和新版显式排序的既有区别。
/// @param m 本次调用借用的只读 map。
/// @param show_info 是否输出元信息。
/// @param out 接收完整 map 文本的流。
/// @param loc 由调用方捕获的位置或占位值。
/// @param indent_width 每层缩进的空格数。
template <typename K, typename V, typename SourceLocation>
void format_map_like(const std::map<K, V> &m, bool show_info, std::ostream &out,
    SourceLocation loc, int indent_width = 4) {
    maybe_print_info(m, show_info, out, loc);
    out << "{\n";
    write_map_entries(m, 1, out, loc, indent_width, sorted_map_layout{});
    out << "\n}" << std::endl;
}

/// 打印顶层 unordered_map，所有标准均按键升序输出。
/// @param m 本次调用借用的只读 unordered_map。
/// @param show_info 是否输出元信息。
/// @param out 接收完整 map 文本的流。
/// @param loc 由调用方捕获的位置或占位值。
/// @param indent_width 每层缩进的空格数。
template <typename K, typename V, typename SourceLocation>
void format_map_like(const std::unordered_map<K, V> &m, bool show_info,
    std::ostream &out, SourceLocation loc, int indent_width = 4) {
    maybe_print_info(m, show_info, out, loc);
    out << "{\n";
    write_map_entries(m, 1, out, loc, indent_width, std::true_type{});
    out << "\n}" << std::endl;
}

/// 在编译期选择 map 布局，避免实例化序列输出。
/// @param c 本次调用借用的只读 map。
/// @param show_info 是否输出元信息。
/// @param out 接收格式化文本的流。
/// @param loc 由调用方捕获的位置或占位值。
/// @param map_tag 表示 map 布局的编译期标记。
template <typename Container, typename SourceLocation>
void format_container_select(const Container &c, bool show_info,
    std::ostream &out, SourceLocation loc, std::true_type /*map_tag*/) {
    format_map_like(c, show_info, out, loc);
}

/// 非 map 容器统一使用序列布局，沿用原有兜底规则。
/// @param c 本次调用借用的只读容器。
/// @param show_info 是否输出元信息。
/// @param out 接收格式化文本的流。
/// @param loc 由调用方捕获的位置或占位值。
/// @param map_tag 表示序列布局的编译期标记。
template <typename Container, typename SourceLocation>
void format_container_select(const Container &c, bool show_info,
    std::ostream &out, SourceLocation loc, std::false_type /*map_tag*/) {
    format_sequence(c, show_info, out, loc);
}

/// 统一的容器打印入口；默认位置在上方声明中捕获，不在本函数内重新取得。
/// @param c 本次调用借用的只读容器。
/// @param show_info 是否输出调用位置和大小。
/// @param loc 调用位置或兼容占位值。
/// @return 独立持有的格式化文本，包含末尾换行。
template <typename Container>
std::string format_container(
    const Container &c, bool show_info, source_location_type loc) {
    std::ostringstream out;
    format_container_select(
        c, show_info, out, loc, typename is_map_like<Container>::type{});
    return out.str();
}

/// 标记参与结构化日志格式化的类型；现有适配器可继续显式特化。
template <typename T>
struct is_supported : std::false_type {};
template <typename T>
struct is_supported<std::vector<T>> : std::true_type {};
template <typename T>
struct is_supported<std::list<T>> : std::true_type {};
template <typename T>
struct is_supported<std::forward_list<T>> : std::true_type {};
template <typename K, typename V>
struct is_supported<std::map<K, V>> : std::true_type {};
template <typename K, typename V>
struct is_supported<std::unordered_map<K, V>> : std::true_type {};

/// 支持的 STL 容器采用结构化布局。
/// @param val 本次调用借用的只读容器。
/// @return 独立持有的格式化文本。
template <typename T>
typename std::enable_if<is_sequence<T>::value || is_map_like<T>::value,
    std::string>::type
format_supported(const T &val) {
    return format_container(val);
}

/// 非容器类型沿用流式输出；缺少 operator<< 时由编译器拒绝。
/// @param val 本次调用借用的只读值。
/// @return 独立持有的格式化文本。
template <typename T>
typename std::enable_if<!is_sequence<T>::value && !is_map_like<T>::value,
    std::string>::type
format_supported(const T &val) {
    std::ostringstream out;
    out << val;
    return out.str();
}

/// 已注册类型通过结构化格式化入口输出。
/// @param val 本次调用借用的只读值。
/// @return 独立持有的格式化文本。
template <typename T>
typename std::enable_if<is_supported<typename std::decay<T>::type>::value,
    std::string>::type
format_value(const T &val) {
    return format_supported(val);
}

/// 其他类型通过流式接口输出，不改变用户类型的注册规则。
/// @param val 本次调用借用的只读值。
/// @return 独立持有的格式化文本。
template <typename T>
typename std::enable_if<!is_supported<typename std::decay<T>::type>::value,
    std::string>::type
format_value(const T &val) {
    std::ostringstream out;
    out << val;
    return out.str();
}

}  // namespace log_detail
}  // namespace mental1104

#endif  // MENTAL1104_LOGGING_DETAIL_CONTAINER_PRINTER_H
