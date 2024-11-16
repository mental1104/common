#include "../include/mental1104/util.h"
#include <iostream>
#include <map>
#include <iostream>
#include <list>
#include <forward_list>
#include <vector>
#include <unordered_map>
#include <string>
#include <thread>
#include <functional>


int add(int a, int b) {
    return a + b;
}

void complicated_operation(){
    std::map<int, int> temp;
    for (int i = 0; i < 100000; i++){
        temp[i] = i;
    }
}

void test_time() {
    mental1104::make_timed(complicated_operation, "complicated_operation")();
    auto temp = mental1104::make_timed(add, "add")(1, 2);
}

void test_print() {
    // 测试普通容器
    std::forward_list<int> flist = {1, 2, 3, 4, 5, 6, 7};
    mental1104::print(flist);

    std::list<int> l = {1, 2, 3, 4, 5, 6, 7};
    mental1104::print(l);

    std::vector<int> v = {1, 2, 3, 4, 5, 6, 7};
    mental1104::print(v);

    // 测试 map
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    mental1104::print(m);

    // 测试 unordered_map
    std::unordered_map<std::string, int> um = {{"x", 10}, {"y", 20}, {"z", 30}};
    mental1104::print(um);

    // 测试嵌套 map
    std::map<std::string, std::map<std::string, int>> nested_map = {
        {"outer1", {{"a", 1}, {"b", 2}}},
        {"outer2", {{"c", 3}, {"d", 4}}}
    };
    mental1104::print(nested_map);

    // 测试嵌套 unordered_map
    std::unordered_map<std::string, std::unordered_map<std::string, int>> nested_umap = {
        {"outer1", {{"a", 1}, {"b", 2}}},
        {"outer2", {{"c", 3}, {"d", 4}}}
    };
    mental1104::print(nested_umap);
}


// 定义函数签名
using TestFunc = std::function<void()>;

int main() {

    std::vector<TestFunc> test_functions = {
        test_time,
        test_print
    };

    for (auto& func: test_functions)
    {
        std::vector<std::thread> threads;
        for(unsigned i = 0; i < 15; ++i)
        {
            threads.emplace_back(std::ref(func));
        }

        for(auto& entry: threads)
            entry.join();
    }
    

    return 0;
}
