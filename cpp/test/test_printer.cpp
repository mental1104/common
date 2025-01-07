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

#include "mental1104/container_printer.h"

TEST(ContainerPrinterTest, PrintForwardList) {
  std::forward_list<int> flist = {1, 2, 3, 4, 5, 6, 7};
  std::ostringstream out;
  // clang-format off
  int line = __LINE__; mental1104::ContainerPrinter::print(flist, out);
  // clang-format on
// forward_list没有 .size() 方法
#if __cplusplus >= 202002L
  EXPECT_EQ(
      out.str(),
      std::format("[File: {}, Line: {}] {{1, 2, 3, 4, 5, 6, 7}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
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
  EXPECT_EQ(
      out.str(),
      std::format("[File: {}, Line: {}] (size: "
                  "7) \n{{1, 2, 3, 4, 5, 6, 7}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
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
  EXPECT_EQ(
      out.str(),
      std::format("[File: {}, Line: {}] (size: 2) \n{{\n   "
                  " \"outer1\": {{\n        \"b\": \"2\",\n        "
                  "\"a\": \"1\"\n    }},\n    \"outer2\": {{\n        "
                  "\"d\": \"4\",\n        \"c\": \"3\"\n    }}\n}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
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
  EXPECT_EQ(
      out.str(),
      std::format("[File: {}, Line: {}] (size: 2) \n{{\n   "
                  " \"outer2\": {{\n        \"d\": \"4\",\n        "
                  "\"c\": \"3\"\n    }},\n    \"outer1\": {{\n        "
                  "\"b\": \"2\",\n        \"a\": \"1\"\n    }}\n}}\n",
                  std::string(std::filesystem::absolute(__FILE__)), line));
#else
  EXPECT_EQ(out.str(),
            "(size: 2) \n{\n    \"outer2\": {\n        \"d\": \"4\",\n        "
            "\"c\": \"3\"\n    },\n    \"outer1\": {\n        \"b\": \"2\",\n  "
            "      \"a\": \"1\"\n    }\n}\n");
#endif
}