#include <gtest/gtest.h>

#include <forward_list>
#include <list>
#include <map>
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
#include "mental1104/container_printer.h"

TEST(ContainerPrinterTest, PrintForwardList) {
  std::forward_list<int> flist = {1, 2, 3, 4, 5, 6, 7};
  std::ostringstream out;
  // clang-format off
    int line = __LINE__; mental1104::ContainerPrinter::print(flist, out);
  // clang-format on
// forward_list没有 .size() 方法
#if __cplusplus >= 202002L
  EXPECT_EQ(out.str(),
            std::format("[File: {}, Line: {}] {{1, 2, 3, 4, 5, 6, 7}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#elif __cplusplus >= 201703L
  EXPECT_EQ(out.str(), "{1, 2, 3, 4, 5, 6, 7}\n");
#else
  EXPECT_EQ(out.str(), "(size: 7) \n{1, 2, 3, 4, 5, 6, 7}\n");
#endif
}

TEST(ContainerPrinterTest, PrintList) {
  std::list<int> l = {1, 2, 3, 4, 5, 6, 7};
  std::ostringstream out;
  // clang-format off
    int line = __LINE__; mental1104::ContainerPrinter::print(l, out);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(
      out.str(),
      std::format("[File: {}, Line: {}] (size: "
                  "7) \n{{1, 2, 3, 4, 5, 6, 7}}\n",
                  std::string(std::string(std::filesystem::absolute(__FILE__))),
                  line));
#else
  EXPECT_EQ(out.str(), "(size: 7) \n{1, 2, 3, 4, 5, 6, 7}\n");
#endif
}

TEST(ContainerPrinterTest, PrintVector) {
  std::vector<int> v = {1, 2, 3, 4, 5, 6, 7};
  std::ostringstream out;
  // clang-format off
    int line = __LINE__; mental1104::ContainerPrinter::print(v, out);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(out.str(),
            std::format("[File: {}, Line: {}] (size: "
                        "7) \n{{1, 2, 3, 4, 5, 6, 7}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#else
  EXPECT_EQ(out.str(), "(size: 7) \n{1, 2, 3, 4, 5, 6, 7}\n");
#endif
}

TEST(ContainerPrinterTest, PrintMap) {
  std::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
  std::ostringstream out;
  // clang-format off
    int line = __LINE__; mental1104::ContainerPrinter::print(m, out);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(
      out.str(),
      std::format("[File: {}, Line: {}] (size: 3) \n{{\n    "
                  "\"a\": \"1\",\n    \"b\": \"2\",\n    \"c\": \"3\"\n}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
#else
  EXPECT_EQ(out.str(),
            "(size: 3) \n{\n    \"a\": \"1\",\n    \"b\": \"2\",\n    \"c\": "
            "\"3\"\n}\n");
#endif

  // 测试嵌套
  out.str("");
  std::map<std::string, std::unordered_map<std::string, int>> nested_map = {
      {"outer1", {{"a", 1}, {"b", 2}}}, {"outer2", {{"c", 3}, {"d", 4}}}};
  // clang-format off
    line = __LINE__; mental1104::ContainerPrinter::print(nested_map, out);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(out.str(),
            std::format("[File: {}, Line: {}] (size: 2) \n{{\n   "
                        " \"outer1\": {{\n        \"b\": \"2\",\n        "
                        "\"a\": \"1\"\n    }},\n    \"outer2\": {{\n        "
                        "\"d\": \"4\",\n        \"c\": \"3\"\n    }}\n}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#else
  EXPECT_EQ(out.str(),
            "(size: 2) \n{\n    \"outer1\": {\n        \"b\": \"2\",\n        "
            "\"a\": \"1\"\n    },\n    \"outer2\": {\n        \"d\": \"4\",\n  "
            "      \"c\": \"3\"\n    }\n}\n");
#endif
}

TEST(ContainerPrinterTest, PrintUnorderMap) {
  // 测试普通unorder_map
  std::unordered_map<std::string, int> m = {{"x", 10}, {"y", 20}, {"z", 30}};
  std::ostringstream out;
  // clang-format off
    int line = __LINE__; mental1104::ContainerPrinter::print(m, out);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(
      out.str(),
      std::format("[File: {}, Line: {}] (size: 3) \n{{\n    "
                  "\"z\": \"30\",\n    \"y\": \"20\",\n    \"x\": \"10\"\n}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
#else
  EXPECT_EQ(out.str(),
            "(size: 3) \n{\n    \"z\": \"30\",\n    \"y\": \"20\",\n    \"x\": "
            "\"10\"\n}\n");
#endif

  // 测试嵌套
  out.str("");
  std::unordered_map<std::string, std::unordered_map<std::string, int>>
      nested_umap = {{"outer1", {{"a", 1}, {"b", 2}}},
                     {"outer2", {{"c", 3}, {"d", 4}}}};
  // clang-format off
    line = __LINE__; mental1104::ContainerPrinter::print(nested_umap, out);
  // clang-format on
#if __cplusplus >= 202002L
  EXPECT_EQ(out.str(),
            std::format("[File: {}, Line: {}] (size: 2) \n{{\n   "
                        " \"outer2\": {{\n        \"d\": \"4\",\n        "
                        "\"c\": \"3\"\n    }},\n    \"outer1\": {{\n        "
                        "\"b\": \"2\",\n        \"a\": \"1\"\n    }}\n}}\n",
                        std::string(std::filesystem::absolute(__FILE__)),
                        line));
#else
  EXPECT_EQ(out.str(),
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

  // 创建一个 stringstream 来捕获输出
  std::stringstream ss;

  // 第一次调用 cache，并打印内部变量
  cache(1); // 应该缓存 (1, 1)
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "1": "1"
}
)");

  ss.str(""); // 清空 stringstream

  // 第二次调用 cache，并打印内部变量
  cache(2); // 应该缓存 (2, 4)
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "2": "4",
    "1": "1"
}
)");

  ss.str(""); // 清空 stringstream

  // 第三次调用 cache，并打印内部变量
  cache(3); // 应该缓存 (3, 9)
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "3": "9",
    "2": "4",
    "1": "1"
}
)");

  ss.str(""); // 清空 stringstream

  // 第四次调用 cache，并打印内部变量
  cache(4); // (1, 1) 应该被淘汰，应该缓存 (4, 16)
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
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

  // 创建一个 stringstream 来捕获输出
  std::stringstream ss;

  // 第一次调用 cache，并打印内部变量
  cache("apple"); // 应该缓存 ("apple", "APPLE")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  // 第二次调用 cache，并打印内部变量
  cache("banana"); // 应该缓存 ("banana", "BANANA")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  // 第三次调用 cache，并打印内部变量
  cache("cherry"); // 应该缓存 ("cherry", "CHERRY")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  // 第四次调用 cache，并打印内部变量
  cache("date"); // 应该缓存 ("date", "DATE")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  // 第五次调用 cache，并打印内部变量
  cache("elderberry"); // 应该缓存 ("elderberry", "ELDERBERRY")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  // 第六次调用 cache，并打印内部变量
  cache("fig"); // 应该缓存 ("fig", "FIG")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  // 第七次调用 cache，并打印内部变量
  cache("grape"); // 应该缓存 ("grape", "GRAPE")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "grape": "GRAPE",
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  // 第八次调用 cache，并打印内部变量，应该淘汰 "apple"
  cache("honeydew"); // ("apple", "APPLE") 应该被淘汰，应该缓存 ("honeydew",
                     // "HONEYDEW")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
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

  // 创建一个 stringstream 来捕获输出
  std::stringstream ss;

  // 添加一些数据到缓存
  cache("apple"); // 应该缓存 ("apple", "APPLE")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  cache("banana"); // 应该缓存 ("banana", "BANANA")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  cache("cherry"); // 应该缓存 ("cherry", "CHERRY")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "cherry": "CHERRY",
    "banana": "BANANA",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  cache("banana"); // 访问 ("banana", "BANANA")，应该移到前面
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "banana": "BANANA",
    "cherry": "CHERRY",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  cache("date"); // 应该缓存 ("date", "DATE")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "date": "DATE",
    "banana": "BANANA",
    "cherry": "CHERRY",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  cache("elderberry"); // 应该缓存 ("elderberry", "ELDERBERRY")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "banana": "BANANA",
    "cherry": "CHERRY",
    "apple": "APPLE"
}
)");

  ss.str(""); // 清空 stringstream

  cache("fig"); // 应该缓存 ("fig", "FIG")
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "banana": "BANANA",
    "cherry": "CHERRY"
}
)");

  ss.str(""); // 清空 stringstream

  // 最后一次访问 "banana" 应该使其移到最前面
  cache("banana"); // 访问 ("banana", "BANANA")，应该移到前面
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
    "banana": "BANANA",
    "fig": "FIG",
    "elderberry": "ELDERBERRY",
    "date": "DATE",
    "cherry": "CHERRY"
}
)");

  ss.str(""); // 清空 stringstream

  // 访问 "apple" 之后，"fig" 应该被淘汰
  cache("apple"); // 应该将 "apple" 移到前面，"fig" 应该被淘汰
  mental1104::ContainerPrinter::print(std::move(cache), ss); // 打印内部变量
  EXPECT_EQ(ss.str(), R"({
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
  std::stringstream ss;

  // 插入元素并检查输出
  cache(1); // 1 -> 1
  mental1104::ContainerPrinter::print(std::move(cache), ss);
  EXPECT_EQ(ss.str(), R"({
    "1": {
        "freq": 1,
        "value": 1
    }
}
)");
  ss.str("");
  ss.clear();

  cache(2); // 2 -> 4
  mental1104::ContainerPrinter::print(std::move(cache), ss);
  EXPECT_EQ(ss.str(), R"({
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
  ss.str("");
  ss.clear();

  cache(3); // 3 -> 9
  mental1104::ContainerPrinter::print(std::move(cache), ss);
  EXPECT_EQ(ss.str(), R"({
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
  ss.str("");
  ss.clear();

  cache(4); // 淘汰 1, 4 -> 16
  mental1104::ContainerPrinter::print(std::move(cache), ss);
  EXPECT_EQ(ss.str(), R"({
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
  ss.str("");
  ss.clear();

  cache(2); // 访问 2, 频率增加
  mental1104::ContainerPrinter::print(std::move(cache), ss);
  EXPECT_EQ(ss.str(), R"({
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

  ss.str("");
  ss.clear();
  // 插入新元素，驱逐一个频率为 1 的元素
  cache(5); // 驱逐 3，5 -> 25
  mental1104::ContainerPrinter::print(std::move(cache), ss);
  EXPECT_EQ(ss.str(), R"({
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
