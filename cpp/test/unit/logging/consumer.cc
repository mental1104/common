/// @file
/// 不链接 GoogleTest 的日志消费验证，始终使用与业务库相同的 C++ 标准。
#include <forward_list>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "mental1104/log.h"

namespace {

/// 记录流式格式化次数；计数器由调用方持有，必须活到日志调用结束。
struct StreamProbe {
    int &writes;
};

/// 记录一次格式化并写入固定消息，供等级过滤检查使用。
/// @param out 接收固定消息的流。
/// @param probe 借用调用方计数器，本次输出使其加一。
/// @return 传入流的引用，不取得所有权。
std::ostream &operator<<(std::ostream &out, const StreamProbe &probe) {
    ++probe.writes;
    return out << "consumer-message";
}

/// 只允许以右值形式进行输出，验证参数拼接不丢失值类别。
struct RvalueMessage {
    /// 创建无资源的测试消息。
    RvalueMessage() = default;
    RvalueMessage(const RvalueMessage &) = delete;
    RvalueMessage &operator=(const RvalueMessage &) = delete;
};

/// 输出临时消息；不需要复制或延长其生命周期。
/// @param out 接收固定消息的流。
/// @param message 本次调用期间有效的右值消息。
/// @return 传入流的引用，不取得所有权。
std::ostream &operator<<(std::ostream &out, RvalueMessage && /*message*/) {
    return out << "rvalue";
}

/// 报告某项可观察契约是否满足，不依赖可能被 NDEBUG 移除的 assert。
/// @param matches 契约是否满足。
/// @param contract 静态 ASCII 契约名称，不保存。
/// @return 满足时为 true，否则写入诊断并返回 false。
bool check(bool matches, const char *contract) {
    if (!matches) std::cerr << "contract failed: " << contract << '\n';
    return matches;
}

}  // namespace

/// 验证公开日志生命周期以及各标准下的容器和消息格式化。
/// @return 所有契约满足时为 0，否则为 1；不保留日志等级修改。
int main() {
    // 最短完整流程：设置等级、输出消息、读取可观察结果、恢复原等级。
    const auto previous = mental1104::get_log_level();
    int writes = 0;
    mental1104::set_log_level(mental1104::LogLevel::Error);
    mental1104::log.debug(StreamProbe{writes});
    const bool filtered = writes == 0;
    mental1104::log.error(StreamProbe{writes});
    const bool emitted = writes == 1 && mental1104::get_log_level() ==
                                            mental1104::LogLevel::Error;
    mental1104::log.errorf("consumer-formatted=%d", 7);
    // 普通方法保留流式自定义类型和右值转发，不要求参数可用于 std::format。
    mental1104::log.error("consumer-", RvalueMessage{});
    mental1104::set_log_level(previous);
    if (!check(filtered && emitted, "public log lifecycle")) return 1;

    // 同一份消费程序验证真实标准，避免 GoogleTest 提升测试驱动的最低标准。
    const std::vector<int> values{1, 2};
    const std::forward_list<int> forward{1, 2};
    const std::unordered_map<std::string, int> entries{{"b", 2}, {"a", 1}};
    if (!check(mental1104::log_detail::format_container(values, false) ==
                   "{1, 2}\n",
            "sequence layout"))
        return 1;
    if (!check(mental1104::log_detail::format_container(forward, false) ==
                   "{1, 2}\n",
            "unsized sequence layout"))
        return 1;
    if (!check(mental1104::log_detail::format_container(entries, false) ==
                   "{\n    \"a\": \"1\",\n    \"b\": \"2\"\n}\n",
            "unordered map order"))
        return 1;

    const auto info = mental1104::log_detail::format_container(forward);
    if (!check(
            (info.find("(size: 2)") != std::string::npos) == !M1104_HAS_CXX17,
            "unsized metadata compatibility"))
        return 1;
    if (!check((info.find("[File: ") != std::string::npos) ==
                   static_cast<bool>(M1104_LOG_HAS_SOURCE_LOCATION),
            "source location capability"))
        return 1;

    const std::map<std::string, std::unordered_map<std::string, int>> nested{
        {"outer", entries}};
    const auto nested_text =
        mental1104::log_detail::format_container(nested, false);
    // C++17 起嵌套 map 也排序；旧标准保留被嵌套容器的迭代顺序。
    const std::string first_key =
        M1104_HAS_CXX17 ? "a" : entries.begin()->first;
    const auto first_position =
        nested_text.find("        \"" + first_key + "\"");
    const auto next_position =
        nested_text.find("        \"", first_position + 1);
    if (!check(first_position != std::string::npos &&
                   next_position != std::string::npos &&
                   first_position < next_position,
            "nested map compatibility"))
        return 1;

    if (!check(mental1104::detail::to_string().empty(), "empty argument pack"))
        return 1;
    if (!check(mental1104::detail::to_string("a", 1, RvalueMessage{}) ==
                   "a1rvalue",
            "append order and forwarding"))
        return 1;
    if (!check(mental1104::detail::format_flexible("value=%d", 7) == "value=7",
            "printf formatting"))
        return 1;
    if (!check(mental1104::detail::format_flexible("%%") == "%" &&
                   mental1104::detail::format_printf("") == " ",
            "zero-argument printf compatibility"))
        return 1;
    const std::string payload(1024, 'x');
    if (!check(
            mental1104::detail::format_printf("%s", payload.c_str()) == payload,
            "writable printf buffer"))
        return 1;
    if (!check(mental1104::detail::format_printf("%s", std::string("text")) ==
                   "%s text",
            "unsafe printf fallback"))
        return 1;

#if M1104_HAS_STD_FORMAT
    if (!check(mental1104::detail::format_flexible("value={}", 7) == "value=7",
            "standard formatting"))
        return 1;
#else
    if (!check(
            mental1104::detail::format_flexible("value={}", 7) == "value={} 7",
            "standard formatting fallback"))
        return 1;
#endif
    return 0;
}
