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
// 判断编译器是否支持 C++20
#if __cplusplus >= 202002L
    #include <source_location>
#else
    struct no_source_location {};  // 在 C++17 中使用一个占位类型
#endif



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


    // Utility function: Check if type has a .size() method
    template <typename T, typename = void>
    struct has_size_method : std::false_type {};

    template <typename T>
    struct has_size_method<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};

    // Print container information (print size if .size() method exists)
    template <typename Container, typename SourceLocation = void>
    void print_info(const Container& c, bool show_info, SourceLocation loc = {}) {
#if __cplusplus >= 202002L
        // If C++20 is supported, use source_location to print file and line
        if (show_info) {
            std::cout << "[File: " << loc.file_name() << ", Line: " << loc.line() << "] ";  // Use file_name() to get file path
            if constexpr (has_size_method<Container>::value) {
                std::cout << "(size: " << c.size() << ") ";
            }
        }
#else
        // If C++20 is not supported, do not print file and line
        if (show_info) {
            if constexpr (has_size_method<Container>::value) {
                std::cout << "(size: " << c.size() << ") ";
            }
        }
#endif
    }

    // Print container (forward_list, list, vector)
    template <typename Container, typename SourceLocation = void>
    void print_internal(const Container& c, bool show_info, SourceLocation loc = {}) {
        print_info(c, show_info, loc);
        std::cout << "{";
        bool first = true;
        for (const auto& element : c) {
            if (!first) std::cout << ", ";
            std::cout << element;
            first = false;
        }
        std::cout << "}" << std::endl;
    }

    // Check if type is a map or unordered_map
    template <typename T>
    struct is_map : std::false_type {};

    template <typename K, typename V>
    struct is_map<std::map<K, V>> : std::true_type {};

    template <typename K, typename V>
    struct is_map<std::unordered_map<K, V>> : std::true_type {};

    // Print map/unordered_map in JSON format
    template <typename K, typename V, typename SourceLocation = void>
    void print_map_or_unordered_map(const K& key, const V& value, bool is_first_element = true, int indent_level = 0, SourceLocation loc = {}) {
        if (!is_first_element) {
            std::cout << ",\n";
        }
        std::cout << std::string(indent_level * 4, ' ') << "\"" << key << "\": ";

        if constexpr (is_map<V>::value) {  // If value is map or unordered_map, recursively process
            std::cout << "{\n";
            bool first = true;
            for (const auto& [nested_key, nested_value] : value) {
                print_map_or_unordered_map(nested_key, nested_value, first, indent_level + 1, loc);
                first = false;
            }
            std::cout << "\n" << std::string(indent_level * 4, ' ') << "}";
        } else {  // Regular type
            std::cout << "\"" << value << "\"";
        }
    }

    // Print map
    template <typename K, typename V, typename SourceLocation = void>
    void print_internal(const std::map<K, V>& m, bool show_info, SourceLocation loc = {}) {
        print_info(m, show_info, loc);
        std::cout << "{\n";
        bool first = true;
        for (const auto& [key, value] : m) {
            print_map_or_unordered_map(key, value, first, 1, loc);
            first = false;
        }
        std::cout << "\n}" << std::endl;
    }

    // Print unordered_map
    template <typename K, typename V, typename SourceLocation = void>
    void print_internal(const std::unordered_map<K, V>& m, bool show_info, SourceLocation loc = {}) {
        print_info(m, show_info, loc);
        std::cout << "{\n";
        bool first = true;
        for (const auto& [key, value] : m) {
            print_map_or_unordered_map(key, value, first, 1, loc);
            first = false;
        }
        std::cout << "\n}" << std::endl;
    }

    // Unified print function, automatically capture file and line numbers
    template <typename Container>
#if __cplusplus >= 202002L
    void print(const Container& c, bool show_info = true, std::source_location loc = std::source_location::current()) {
#else
    void print(const Container& c, bool show_info = true, no_source_location loc = no_source_location()) {
#endif
        print_internal(c, show_info, loc);
    }
}

#endif
