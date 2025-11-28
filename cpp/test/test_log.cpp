#include <gtest/gtest.h>

#include <forward_list>
#include <list>
#include <map>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>
// 如果C++版本大于等于C++20，则将format库引入
#if __cplusplus >= 202002L
#include <format>
#endif

#if __cplusplus >= 201703L
#include <filesystem>
#endif

#include "mental1104/cache.h"
#include "mental1104/log.h"
#include "mental1104/log/adapters/cache_printer.h"

using mental1104::LFUCache;
using mental1104::LRUCache;
using mental1104::make_cache;
using mental1104::make_lfu_cache;
using mental1104::make_lru_cache;

TEST(ContainerPrinterTest, PrintForwardList) {
  std::forward_list<int> flist = {1, 2, 3, 4, 5, 6, 7};
  // clang-format off
    int line = __LINE__; auto out = mental1104::log_detail::format_container(flist);
  // clang-format on
// forward_list没有 .size() 方法
#if __cplusplus >= 202002L
  EXPECT_EQ(out,
            std::format("[File: {}, Line: {}] {{1, 2, 3, 4, 5, 6, 7}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#elif __cplusplus >= 201703L
  EXPECT_EQ(out, "{1, 2, 3, 4, 5, 6, 7}\n");
#else
  EXPECT_EQ(out, "(size: 7) \n{1, 2, 3, 4, 5, 6, 7}\n");
#endif
}

TEST(ContainerPrinterTest, PrintList) {
  std::list<int> l = {1, 2, 3, 4, 5, 6, 7};
  // clang-format off
    int line = __LINE__; auto out = mental1104::log_detail::format_container(l);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(
      out,
      std::format("[File: {}, Line: {}] (size: "
                  "7) \n{{1, 2, 3, 4, 5, 6, 7}}\n",
                  std::string(std::string(std::filesystem::absolute(__FILE__))),
                  line));
#else
  EXPECT_EQ(out, "(size: 7) \n{1, 2, 3, 4, 5, 6, 7}\n");
#endif
}

TEST(ContainerPrinterTest, PrintVector) {
  std::vector<int> v = {1, 2, 3, 4, 5, 6, 7};
  // clang-format off
    int line = __LINE__; auto out = mental1104::log_detail::format_container(v);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(out,
            std::format("[File: {}, Line: {}] (size: "
                        "7) \n{{1, 2, 3, 4, 5, 6, 7}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#else
  EXPECT_EQ(out, "(size: 7) \n{1, 2, 3, 4, 5, 6, 7}\n");
#endif
}

TEST(ContainerPrinterTest, PrintMap) {
  std::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
  // clang-format off
    int line = __LINE__; auto out = mental1104::log_detail::format_container(m);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(
      out,
      std::format("[File: {}, Line: {}] (size: 3) \n{{\n    "
                  "\"a\": \"1\",\n    \"b\": \"2\",\n    \"c\": \"3\"\n}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
#else
  EXPECT_EQ(out,
            "(size: 3) \n{\n    \"a\": \"1\",\n    \"b\": \"2\",\n    \"c\": "
            "\"3\"\n}\n");
#endif

  // 测试嵌套
  std::map<std::string, std::unordered_map<std::string, int>> nested_map = {
      {"outer1", {{"a", 1}, {"b", 2}}}, {"outer2", {{"c", 3}, {"d", 4}}}};
  // clang-format off
    line = __LINE__; auto nested_out = mental1104::log_detail::format_container(nested_map);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(nested_out,
            std::format("[File: {}, Line: {}] (size: 2) \n{{\n   "
                        " \"outer1\": {{\n        \"b\": \"2\",\n        "
                        "\"a\": \"1\"\n    }},\n    \"outer2\": {{\n        "
                        "\"d\": \"4\",\n        \"c\": \"3\"\n    }}\n}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#else
  EXPECT_EQ(nested_out,
            "(size: 2) \n{\n    \"outer1\": {\n        \"b\": \"2\",\n        "
            "\"a\": \"1\"\n    },\n    \"outer2\": {\n        \"d\": \"4\",\n  "
            "      \"c\": \"3\"\n    }\n}\n");
#endif
}

TEST(ContainerPrinterTest, PrintUnorderMap) {
  // 测试普通unorder_map
  std::unordered_map<std::string, int> m = {{"x", 10}, {"y", 20}, {"z", 30}};
  // clang-format off
    int line = __LINE__; auto out = mental1104::log_detail::format_container(m);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(
      out,
      std::format("[File: {}, Line: {}] (size: 3) \n{{\n    "
                  "\"z\": \"30\",\n    \"y\": \"20\",\n    \"x\": \"10\"\n}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
#else
  EXPECT_EQ(out,
            "(size: 3) \n{\n    \"z\": \"30\",\n    \"y\": \"20\",\n    \"x\": "
            "\"10\"\n}\n");
#endif

  // 测试嵌套
  std::unordered_map<std::string, std::unordered_map<std::string, int>>
      nested_umap = {{"outer1", {{"a", 1}, {"b", 2}}},
                     {"outer2", {{"c", 3}, {"d", 4}}}};
  // clang-format off
    line = __LINE__; auto nested_out = mental1104::log_detail::format_container(nested_umap);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(nested_out,
            std::format("[File: {}, Line: {}] (size: 2) \n{{\n   "
                        " \"outer2\": {{\n        \"d\": \"4\",\n        "
                        "\"c\": \"3\"\n    }},\n    \"outer1\": {{\n        "
                        "\"b\": \"2\",\n        \"a\": \"1\"\n    }}\n}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#else
  EXPECT_EQ(nested_out,
            "(size: 2) \n{\n    \"outer2\": {\n        \"d\": \"4\",\n        "
            "\"c\": \"3\"\n    },\n    \"outer1\": {\n        \"b\": \"2\",\n  "
            "      \"a\": \"1\"\n    }\n}\n");
#endif
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
    "2": {
        "freq": 1,
        "value": 4
    },
    "1": {
        "freq": 1,
        "value": 1
    }
}
)");

  cache(3); // 3 -> 9
  auto out3 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out3, R"({
    "3": {
        "freq": 1,
        "value": 9
    },
    "2": {
        "freq": 1,
        "value": 4
    },
    "1": {
        "freq": 1,
        "value": 1
    }
}
)");

  cache(4); // 淘汰 1, 4 -> 16
  auto out4 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out4, R"({
    "4": {
        "freq": 1,
        "value": 16
    },
    "3": {
        "freq": 1,
        "value": 9
    },
    "2": {
        "freq": 1,
        "value": 4
    }
}
)");

  cache(2); // 访问 2, 频率增加
  auto out5 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out5, R"({
    "4": {
        "freq": 1,
        "value": 16
    },
    "3": {
        "freq": 1,
        "value": 9
    },
    "2": {
        "freq": 2,
        "value": 4
    }
}
)");

  // 插入新元素，驱逐一个频率为 1 的元素
  cache(5); // 驱逐 3，5 -> 25
  auto out6 = mental1104::log_detail::format_value(cache);
  EXPECT_EQ(out6, R"({
    "5": {
        "freq": 1,
        "value": 25
    },
    "4": {
        "freq": 1,
        "value": 16
    },
    "2": {
        "freq": 2,
        "value": 4
    }
}
)");
}
