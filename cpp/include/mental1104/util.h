#ifndef MENTAL1104_UTIL
#define MENTAL1104_UTIL

#include <iostream>
#include <functional>
#include <string>
#include <chrono>
#include <iomanip>
#include <typeinfo>
#include <iostream>
#include <list>
#include <forward_list>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <iterator>
#include <type_traits>

namespace mental1104 {

    // 主模板声明
    template<typename T>
    class Timed;

    // 针对函数类型 T = R(Args...) 的部分特化
    template<typename R, typename... Args>
    class Timed<R(Args...)> {
    public:
        Timed(std::function <R(Args...)> func, std::string name) : func{std::move(func)}, name{std::move(name)} {}
        R operator() (Args... args) {
            std::cout << "Entering " << name << '\n';
            auto start = std::chrono::high_resolution_clock::now();
            R result = func(args...);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end - start;
            std::cout << "Exiting " << name << " with " << std::fixed << std::setprecision(10) << duration.count() << " seconds" << std::endl;;
            return result;
        }
    private:
        std::function<R(Args...)> func;
        std::string name;
    };

    // 针对返回类型为void的函数进行部分特化
    template<typename... Args>
    class Timed<void(Args...)> {
    public:
        Timed(std::function<void(Args...)> func, std::string name) : func{std::move(func)}, name{std::move(name)} {}

        void operator()(Args... args) {
            std::cout << "Entering " << name << '\n';
            auto start = std::chrono::high_resolution_clock::now();
            func(args...);  // 不需要保存返回值
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end - start;
            std::cout << "Exiting " << (name.size() == 0 ? "with " : name + " with ") << std::fixed << std::setprecision(10) << duration.count() << " seconds" << std::endl;
        }

    private:
        std::function<void(Args...)> func;
        std::string name;
    };

    template<typename R, typename... Args>
    auto make_timed(R (*func)(Args...), const std::string & name = std::string()) {
        return Timed<R(Args...)>(std::function<R(Args...)>(func), name);
    }

    // 针对返回类型为void的函数的辅助函数
    template<typename... Args>
    auto make_timed(void (*func)(Args...), const std::string &name = std::string()) {
        return Timed<void(Args...)>(std::function<void(Args...)>(func), name);
    }


    // 工具函数：判断类型是否为 map 或 unordered_map
    template <typename T>
    struct is_map : std::false_type {};

    template <typename K, typename V>
    struct is_map<std::map<K, V>> : std::true_type {};

    template <typename K, typename V>
    struct is_map<std::unordered_map<K, V>> : std::true_type {};

    // 打印 forward_list, list, vector 格式
    template <typename Container>
    void print(const Container& c) {
        std::cout << "{";
        bool first = true;
        for (const auto& element : c) {
            if (!first) std::cout << ", ";
            std::cout << element;
            first = false;
        }
        std::cout << "}" << std::endl;
    }

    // 打印 map/unordered_map 为 JSON 格式
    template <typename K, typename V>
    void print_map_or_unordered_map(const K& key, const V& value, bool is_first_element = true, int indent_level = 0) {
        if (!is_first_element) {
            std::cout << ",\n";
        }
        std::cout << std::string(indent_level * 4, ' ') << "\"" << key << "\": ";

        if constexpr (is_map<V>::value) {  // 如果 value 是 map 或 unordered_map，则递归处理
            std::cout << "{\n";
            bool first = true;
            for (const auto& [nested_key, nested_value] : value) {
                print_map_or_unordered_map(nested_key, nested_value, first, indent_level + 1);
                first = false;
            }
            std::cout << "\n" << std::string(indent_level * 4, ' ') << "}";
        } else {  // 普通类型
            std::cout << "\"" << value << "\"";
        }
    }

    // 打印 map
    template <typename K, typename V>
    void print(const std::map<K, V>& m) {
        std::cout << "{\n";
        bool first = true;
        for (const auto& [key, value] : m) {
            print_map_or_unordered_map(key, value, first, 1);
            first = false;
        }
        std::cout << "\n}" << std::endl;
    }

    // 打印 unordered_map
    template <typename K, typename V>
    void print(const std::unordered_map<K, V>& m) {
        std::cout << "{\n";
        bool first = true;
        for (const auto& [key, value] : m) {
            print_map_or_unordered_map(key, value, first, 1);
            first = false;
        }
        std::cout << "\n}" << std::endl;
    }
}

#endif
