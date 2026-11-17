#include <gtest/gtest.h>

#include <algorithm>
#include <forward_list>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "mental1104/c_api_raii.h"
#include "mental1104/core/cache.h"
#include "mental1104/log.h"
#include "mental1104/logging/adapters/cache_printer.h"

using mental1104::LFUCache;
using mental1104::LRUCache;
using mental1104::make_cache;
using mental1104::make_lfu_cache;
using mental1104::make_lru_cache;

/// 最小完整用法：设置等级、输出普通和格式化消息、读取结果并恢复全局状态。
TEST(LogTest, WriteMessagesThroughPublicApi) {
    const auto previous = mental1104::get_log_level();
    auto restore = mental1104::make_scope_exit(
        [previous] { mental1104::set_log_level(previous); });
    mental1104::set_log_level(mental1104::LogLevel::Info);

    // 同时捕获两条输出流，兼容 spdlog 以及 stdout/stderr 回退后端。
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    mental1104::log.info("ready: ", 1);
    mental1104::log.warningf("retry=%d", 3);
    mental1104::log.warning("cache is full");
    mental1104::log.errorf("code=%d", 500);
    mental1104::log.debug("filtered-message");
    const auto error_output = testing::internal::GetCapturedStderr();
    const auto output = testing::internal::GetCapturedStdout() + error_output;

    EXPECT_NE(std::string::npos, output.find("ready: 1"));
    EXPECT_NE(std::string::npos, output.find("retry=3"));
    EXPECT_NE(std::string::npos, output.find("cache is full"));
    EXPECT_NE(std::string::npos, output.find("code=500"));
    EXPECT_EQ(std::string::npos, output.find("filtered-message"));
    EXPECT_EQ(mental1104::LogLevel::Info, mental1104::get_log_level());
}

/// 四个等级的方法和现有宏必须使用同一过滤阈值，拼接与格式化分别验证。
TEST(LogTest, LevelMethodsAndMacrosUseSharedFilter) {
    using mental1104::LogLevel;
    const auto previous = mental1104::get_log_level();
    auto restore = mental1104::make_scope_exit(
        [previous] { mental1104::set_log_level(previous); });
    for (const auto threshold :
        {LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error}) {
        SCOPED_TRACE(static_cast<int>(threshold));
        mental1104::set_log_level(threshold);
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        mental1104::log.debug("method-debug-raw");
        mental1104::log.info("method-info-raw");
        mental1104::log.warning("method-warning-raw");
        mental1104::log.error("method-error-raw");
        mental1104::log.debugf("method-debug-format=%d", 7);
        mental1104::log.infof("method-info-format=%d", 7);
        mental1104::log.warningf("method-warning-format=%d", 7);
        mental1104::log.errorf("method-error-format=%d", 7);
        M1104_LOG_DEBUG("macro-debug-raw");
        M1104_LOG_INFO("macro-info-raw");
        M1104_LOG_WARNING("macro-warning-raw");
        M1104_LOG_ERROR("macro-error-raw");
        M1104_LOG_DEBUGF("macro-debug-format=%d", 7);
        M1104_LOG_INFOF("macro-info-format=%d", 7);
        M1104_LOG_WARNINGF("macro-warning-format=%d", 7);
        M1104_LOG_ERRORF("macro-error-format=%d", 7);
        const auto errors = testing::internal::GetCapturedStderr();
        const auto output = testing::internal::GetCapturedStdout() + errors;

        // 每个阈值下独立核对等级绑定，防止某个方法错误地转发成另一个等级。
        for (const std::string source : {"method-", "macro-"}) {
            for (const std::string kind : {"-raw", "-format=7"}) {
                EXPECT_EQ(threshold == LogLevel::Debug,
                    output.find(source + "debug" + kind) != std::string::npos);
                EXPECT_EQ(
                    threshold == LogLevel::Debug || threshold == LogLevel::Info,
                    output.find(source + "info" + kind) != std::string::npos);
                EXPECT_EQ(threshold != LogLevel::Error,
                    output.find(source + "warning" + kind) !=
                        std::string::npos);
                EXPECT_NE(
                    std::string::npos, output.find(source + "error" + kind));
            }
        }
    }
}

/// 普通方法不解释占位符，f 后缀方法才按格式规则处理，且过滤先于格式化。
TEST(LogTest, PlainAndFormattedMethodsStayDistinct) {
    const auto previous = mental1104::get_log_level();
    auto restore = mental1104::make_scope_exit(
        [previous] { mental1104::set_log_level(previous); });
    mental1104::set_log_level(mental1104::LogLevel::Info);
    testing::internal::CaptureStdout();
    mental1104::log.info("literal %d {} ", 7);
    mental1104::log.infof("formatted=%d", 7);
    M1104_LOG_INFOF("zero-argument-format");
    EXPECT_NO_THROW(mental1104::log.debugf("{", 7));
    const auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(std::string::npos, output.find("literal %d {} 7"));
    EXPECT_NE(std::string::npos, output.find("formatted=7"));
    EXPECT_NE(std::string::npos, output.find("zero-argument-format"));
    EXPECT_EQ(std::string::npos, output.find("{ 7"));
#if M1104_HAS_STD_FORMAT
    EXPECT_THROW(mental1104::log.errorf("{", 7), std::format_error);
#endif
}

/// 隐藏元信息时，各标准均保留相同的容器布局和顶层无序 map 排序。
TEST(ContainerPrinterTest, HiddenInfoPreservesLayout) {
    const std::vector<int> values{1, 2};
    const std::forward_list<int> forward{1, 2};
    const std::unordered_map<std::string, int> entries{{"b", 2}, {"a", 1}};
    EXPECT_EQ(
        "{1, 2}\n", mental1104::log_detail::format_container(values, false));
    EXPECT_EQ(
        "{1, 2}\n", mental1104::log_detail::format_container(forward, false));
    EXPECT_EQ("{\n    \"a\": \"1\",\n    \"b\": \"2\"\n}\n",
        mental1104::log_detail::format_container(entries, false));
}

/// 格式选择保持既有优先级；不安全 printf 对象继续走拼接回退。
TEST(LogTest, FormattingPreservesBackendSemantics) {
    EXPECT_EQ("value=7", mental1104::detail::format_flexible("value=%d", 7));
    EXPECT_EQ("%", mental1104::detail::format_flexible("%%"));
    EXPECT_EQ(" ", mental1104::detail::format_printf(""));
    EXPECT_EQ("%s text",
        mental1104::detail::format_printf("%s", std::string("text")));
    EXPECT_EQ("", mental1104::detail::to_string());
    EXPECT_EQ("a1b2", mental1104::detail::to_string("a", 1, "b", 2));
#if M1104_HAS_STD_FORMAT
    EXPECT_EQ("value=7", mental1104::detail::format_flexible("value={}", 7));
    EXPECT_EQ(
        "value=7 %%", mental1104::detail::format_flexible("value={} %%", 7));
    EXPECT_EQ("plain", mental1104::detail::format_flexible("plain"));
    EXPECT_THROW(
        mental1104::detail::format_flexible("{", 7), std::format_error);
#else
    EXPECT_EQ("value={} 7", mental1104::detail::format_flexible("value={}", 7));
    EXPECT_EQ(
        "value={} %", mental1104::detail::format_flexible("value={} %%", 7));
    EXPECT_EQ("plain ", mental1104::detail::format_flexible("plain"));
    EXPECT_EQ("{ 7", mental1104::detail::format_flexible("{", 7));
#endif
}

namespace {
/// 构造独立的预期元信息，不依赖被测 std::format 或 filesystem 支持。
/// @param line 默认实参应捕获的调用行。
/// @param size 预期元素数；负值表示不应输出大小。
/// @return 预期的位置与大小前缀。
std::string expected_container_prefix(int line, int size) {
    std::string prefix;
#if M1104_LOG_HAS_SOURCE_LOCATION
    prefix = std::string("[File: ") + __FILE__ +
             ", Line: " + std::to_string(line) + "] ";
#else
    (void)line;
#endif
    if (size >= 0) prefix += "(size: " + std::to_string(size) + ") \n";
    return prefix;
}
}  // namespace

/// forward_list 没有 size()，旧标准保留计数，新版省略大小。
TEST(ContainerPrinterTest, PrintForwardList) {
    const std::forward_list<int> values{1, 2, 3, 4, 5, 6, 7};
    // clang-format off
    const int line = __LINE__; const auto out = mental1104::log_detail::format_container(values);
    // clang-format on
    EXPECT_EQ(expected_container_prefix(line, M1104_HAS_CXX17 ? -1 : 7) +
                  "{1, 2, 3, 4, 5, 6, 7}\n",
        out);
}

/// 有 size() 的链表始终输出大小，位置必须对应本次调用点。
TEST(ContainerPrinterTest, PrintList) {
    const std::list<int> values{1, 2, 3, 4, 5, 6, 7};
    // clang-format off
    const int line = __LINE__; const auto out = mental1104::log_detail::format_container(values);
    // clang-format on
    EXPECT_EQ(
        expected_container_prefix(line, 7) + "{1, 2, 3, 4, 5, 6, 7}\n", out);
}

/// vector 与其他有大小的序列共享布局，不改变默认调用位置。
TEST(ContainerPrinterTest, PrintVector) {
    const std::vector<int> values{1, 2, 3, 4, 5, 6, 7};
    // clang-format off
    const int line = __LINE__; const auto out = mental1104::log_detail::format_container(values);
    // clang-format on
    EXPECT_EQ(
        expected_container_prefix(line, 7) + "{1, 2, 3, 4, 5, 6, 7}\n", out);
}

/// map 和嵌套 map 保留原有引号、缩进及调用方元信息。
TEST(ContainerPrinterTest, PrintMap) {
    const std::map<std::string, int> values{{"a", 1}, {"b", 2}, {"c", 3}};
    // clang-format off
    const int line = __LINE__; const auto out = mental1104::log_detail::format_container(values);
    // clang-format on
    EXPECT_EQ(
        expected_container_prefix(line, 3) +
            "{\n    \"a\": \"1\",\n    \"b\": \"2\",\n    \"c\": \"3\"\n}\n",
        out);

    const std::map<std::string, std::unordered_map<std::string, int>> nested{
        {"outer1", {{"a", 1}, {"b", 2}}}, {"outer2", {{"c", 3}, {"d", 4}}}};
    // clang-format off
    const int nested_line = __LINE__; const auto nested_out = mental1104::log_detail::format_container(nested);
    // clang-format on
    EXPECT_EQ(expected_container_prefix(nested_line, 2) +
                  "{\n    \"outer1\": {\n        \"a\": \"1\",\n        \"b\": "
                  "\"2\"\n"
                  "    },\n    \"outer2\": {\n        \"c\": \"3\",\n        "
                  "\"d\": \"4\"\n    }\n}\n",
        nested_out);
}

/// 顶层和新版嵌套 unordered_map 都按键排序，输出不依赖哈希迭代顺序。
TEST(ContainerPrinterTest, PrintUnorderMap) {
    const std::unordered_map<std::string, int> values{
        {"x", 10}, {"y", 20}, {"z", 30}};
    // clang-format off
    const int line = __LINE__; const auto out = mental1104::log_detail::format_container(values);
    // clang-format on
    EXPECT_EQ(
        expected_container_prefix(line, 3) +
            "{\n    \"x\": \"10\",\n    \"y\": \"20\",\n    \"z\": \"30\"\n}\n",
        out);

    const std::unordered_map<std::string, std::unordered_map<std::string, int>>
        nested{
            {"outer1", {{"a", 1}, {"b", 2}}}, {"outer2", {{"c", 3}, {"d", 4}}}};
    // clang-format off
    const int nested_line = __LINE__; const auto nested_out = mental1104::log_detail::format_container(nested);
    // clang-format on
    EXPECT_EQ(expected_container_prefix(nested_line, 2) +
                  "{\n    \"outer1\": {\n        \"a\": \"1\",\n        \"b\": "
                  "\"2\"\n"
                  "    },\n    \"outer2\": {\n        \"c\": \"3\",\n        "
                  "\"d\": \"4\"\n    }\n}\n",
        nested_out);
}

// 测试 LRUCache 的打印逻辑
TEST(LRUCacheTest, PrintInternal) {
  // 使用一个简单的缓存函数，传递 lambda 表达式
  auto square = [](int x) { return x * x; };

  // 创建一个容量为 3 的 LRUCache，使用这个函数
  auto cache = make_lru_cache<int, int>(3, square);

  // 第一次调用 cache，并打印内部变量
  cache(1); // 应该缓存 (1, 1)
  auto out1 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out1, R"({
    "1": "1"
}
)");

  // 第二次调用 cache，并打印内部变量
  cache(2); // 应该缓存 (2, 4)
  auto out2 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out2, R"({
    "2": "4",
    "1": "1"
}
)");

  // 第三次调用 cache，并打印内部变量
  cache(3); // 应该缓存 (3, 9)
  auto out3 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out3, R"({
    "3": "9",
    "2": "4",
    "1": "1"
}
)");

  // 第四次调用 cache，并打印内部变量
  cache(4); // (1, 1) 应该被淘汰，应该缓存 (4, 16)
  auto out4 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out4, R"({
    "4": "16",
    "3": "9",
    "2": "4"
}
)");
}

// 测试 print_internal 的逻辑，字符串版本，缓存大小调整为 7
TEST(LRUCacheTest, PrintInternalString) {
  // 使用一个将字符串转换为大写的缓存函数，传递 lambda 表达式
  auto to_upper = [](const std::string &str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
  };

  // 创建一个容量为 7 的 LRUCache，使用这个函数
  auto cache = make_lru_cache<std::string, std::string>(7, to_upper);

  // 第一次调用 cache，并打印内部变量
  cache("apple"); // 应该缓存 ("apple", "APPLE")
  auto out1 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out1, R"({
    "apple": "APPLE"
}
)");

  // 第二次调用 cache，并打印内部变量
  cache("banana"); // 应该缓存 ("banana", "BANANA")
  auto out2 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out2, R"({
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  // 第三次调用 cache，并打印内部变量
  cache("cherry"); // 应该缓存 ("cherry", "CHERRY")
  auto out3 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out3, R"({
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  // 第四次调用 cache，并打印内部变量
  cache("date"); // 应该缓存 ("date", "DATE")
  auto out4 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out4, R"({
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  // 第五次调用 cache，并打印内部变量
  cache("elderberry"); // 应该缓存 ("elderberry", "ELDERBERRY")
  auto out5 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out5, R"({
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  // 第六次调用 cache，并打印内部变量
  cache("fig"); // 应该缓存 ("fig", "FIG")
  auto out6 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out6, R"({
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  // 第七次调用 cache，并打印内部变量
  cache("grape"); // 应该缓存 ("grape", "GRAPE")
  auto out7 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out7, R"({
    "grape": "GRAPE",
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  // 第八次调用 cache，并打印内部变量，应该淘汰 "apple"
  cache("honeydew"); // ("apple", "APPLE") 应该被淘汰，应该缓存 ("honeydew",
                     // "HONEYDEW")
  auto out8 = mental1104::log_detail::format_value(cache); // 打印内部变量
  EXPECT_EQ(out8, R"({
    "honeydew": "HONEYDEW",
    "grape": "GRAPE",
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA"
}
)");
}

// 测试 LRUCache 的重复元素移到前面功能
TEST(LRUCacheTest, PrintInternalWithLRUBehavior) {
  // 使用一个将字符串转换为大写的缓存函数，传递 lambda 表达式
  auto to_upper = [](const std::string &str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
  };

  // 创建一个容量为 5 的 LRUCache，使用这个函数
  auto cache = make_lru_cache<std::string, std::string>(5, to_upper);

  cache("apple");
  auto out1 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out1, R"({
    "apple": "APPLE"
}
)");

  cache("banana");
  auto out2 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out2, R"({
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  cache("cherry");
  auto out3 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out3, R"({
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  cache("banana");
  auto out4 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out4, R"({
    "banana": "BANANA",
    "cherry": "CHERRY",
    "apple": "APPLE"
}
)");

  cache("date");
  auto out5 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out5, R"({
    "date": "DATE",
    "banana": "BANANA",
    "cherry": "CHERRY",
    "apple": "APPLE"
}
)");

  cache("elderberry");
  auto out6 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out6, R"({
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "banana": "BANANA",
    "cherry": "CHERRY",
    "apple": "APPLE"
}
)");

  cache("fig");
  auto out7 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out7, R"({
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "banana": "BANANA",
    "cherry": "CHERRY"
}
)");

  cache("banana");
  auto out8 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out8, R"({
    "banana": "BANANA",
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY"
}
)");

  cache("apple");
  auto out9 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out9, R"({
    "apple": "APPLE",
    "banana": "BANANA",
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE"
}
)");
}

TEST(LFUCacheTest, PrintInternal) {
  auto square = [](int x) { return x * x; };
  auto cache = make_lfu_cache<int, int>(3, square);

  // 插入元素并检查输出
  cache(1); // 1 -> 1
  auto out1 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out1, R"({
    "1": {
        "freq": 1,
        "value": 1
    }
}
)");

  cache(2); // 2 -> 4
  auto out2 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out2, R"({
    "1": {
        "freq": 1,
        "value": 1
    },
    "2": {
        "freq": 1,
        "value": 4
    }
}
)");

  cache(3); // 3 -> 9
  auto out3 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out3, R"({
    "1": {
        "freq": 1,
        "value": 1
    },
    "2": {
        "freq": 1,
        "value": 4
    },
    "3": {
        "freq": 1,
        "value": 9
    }
}
)");

  cache(4); // 淘汰 1, 4 -> 16
  auto out4 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out4, R"({
    "2": {
        "freq": 1,
        "value": 4
    },
    "3": {
        "freq": 1,
        "value": 9
    },
    "4": {
        "freq": 1,
        "value": 16
    }
}
)");

  cache(2); // 访问 2, 频率增加
  auto out5 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out5, R"({
    "2": {
        "freq": 2,
        "value": 4
    },
    "3": {
        "freq": 1,
        "value": 9
    },
    "4": {
        "freq": 1,
        "value": 16
    }
}
)");

  // 插入新元素，驱逐一个频率为 1 的元素
  cache(5); // 驱逐 3，5 -> 25
  auto out6 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out6, R"({
    "2": {
        "freq": 2,
        "value": 4
    },
    "4": {
        "freq": 1,
        "value": 16
    },
    "5": {
        "freq": 1,
        "value": 25
    }
}
)");
}

#undef M1104_LOG_TEST_MARK_LINE_USED
