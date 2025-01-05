#include <gtest/gtest.h>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <string>
#include "mental1104/util.h"  // 包含你定义的 mental1104::contains


// 测试容器内是否包含特定元素
TEST(ContainsTest, VectorTest) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    EXPECT_TRUE(mental1104::contains(vec, 3));  // 测试元素 3 存在
    EXPECT_FALSE(mental1104::contains(vec, 6)); // 测试元素 6 不存在
}

TEST(ContainsTest, ListTest) {
    std::list<std::string> str_list = {"apple", "banana", "cherry"};
    EXPECT_TRUE(mental1104::contains(str_list, "banana"));  // 测试字符串 "banana" 存在
    EXPECT_FALSE(mental1104::contains(str_list, "orange")); // 测试字符串 "orange" 不存在
}

TEST(ContainsTest, SetTest) {
    std::set<int> int_set = {10, 20, 30, 40};
    EXPECT_TRUE(mental1104::contains(int_set, 30));  // 测试元素 30 存在
    EXPECT_FALSE(mental1104::contains(int_set, 50)); // 测试元素 50 不存在
}

TEST(ContainsTest, MapTest) {
    std::map<int, std::string> num_map = {{1, "one"}, {2, "two"}, {3, "three"}};
    EXPECT_TRUE(mental1104::contains(num_map, 2));  // 测试键 2 存在
    EXPECT_FALSE(mental1104::contains(num_map, 4)); // 测试键 4 不存在
}

TEST(ContainsTest, EmptyContainerTest) {
    std::vector<int> empty_vec;
    EXPECT_FALSE(mental1104::contains(empty_vec, 1)); // 测试空容器，元素不存在
}
