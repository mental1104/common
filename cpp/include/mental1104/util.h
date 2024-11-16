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


    // 打印容器通用模板
    template <typename Container>
    void print(const Container& c);

    // 特化: 打印 forward_list, list, vector 格式
    template <typename T>
    void print(const std::forward_list<T>& c) {
        std::cout << "{";
        for (auto it = c.begin(); it != c.end(); ++it) {
            if (it != c.begin()) std::cout << ", ";
            std::cout << *it;
        }
        std::cout << "}" << std::endl;
    }

    template <typename T>
    void print(const std::list<T>& c) {
        std::cout << "{";
        for (auto it = c.begin(); it != c.end(); ++it) {
            if (it != c.begin()) std::cout << ", ";
            std::cout << *it;
        }
        std::cout << "}" << std::endl;
    }

    template <typename T>
    void print(const std::vector<T>& c) {
        std::cout << "{";
        for (size_t i = 0; i < c.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << c[i];
        }
        std::cout << "}" << std::endl;
    }

    // 递归打印 map 或 unordered_map 为 JSON 格式
    template <typename K, typename V>
    void print_map_or_unordered_map(const K& key, const V& value, bool is_first_element = true, int indent_level = 0) {
        // 在逗号前打印适当的空格
        if (!is_first_element) {
            std::cout << ",\n";
        }
        
        // 打印 key
        std::cout << std::string(indent_level * 4, ' ') << "\"" << key << "\": ";

        // 如果 value 本身是容器（map 或 unordered_map），则递归调用 print
        if constexpr (std::is_same_v<V, std::map<typename V::key_type, typename V::mapped_type>> || 
                    std::is_same_v<V, std::unordered_map<typename V::key_type, typename V::mapped_type>>) {
            std::cout << "{";
            bool first = true;
            for (const auto& [nested_key, nested_value] : value) {
                print_map_or_unordered_map(nested_key, nested_value, first, indent_level + 1);
                first = false;
            }
            std::cout << "\n" << std::string(indent_level * 4, ' ') << "}";
        } else {
            // 打印普通值
            std::cout << "\"" << value << "\"";
        }
    }

    // 特化: 打印 map 和 unordered_map 格式为 JSON 格式
    template <typename K, typename V>
    void print(const std::map<K, V>& m) {
        std::cout << "{\n";
        bool first = true;
        for (const auto& [key, value] : m) {
            print_map_or_unordered_map(key, value, first);
            first = false;
        }
        std::cout << "\n}" << std::endl;
    }

    template <typename K, typename V>
    void print(const std::unordered_map<K, V>& m) {
        std::cout << "{\n";
        bool first = true;
        for (const auto& [key, value] : m) {
            print_map_or_unordered_map(key, value, first);
            first = false;
        }
        std::cout << "\n}" << std::endl;
    }
}

#endif
