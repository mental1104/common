#ifndef __MENTAL1104_CONTAINR_PRINTER
#define __MENTAL1104_CONTAINR_PRINTER

#include <list>
#include <forward_list>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <type_traits>
#include <mutex>

// 判断编译器是否支持 C++20
#if __cplusplus >= 202002L
    #include <source_location>
#else
    struct no_source_location {};  // 在 C++17 中使用一个占位类型
#endif

namespace mental1104 {
    class ContainerPrinter {
    private:
        static std::mutex print_mutex;
        template <typename T, typename = void>
        struct has_size_method : std::false_type {};

    #if __cplusplus >= 201703L
        template <typename T>
        struct has_size_method<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};
    #else

        // Helper function for containers with size()
        template <typename Container>
        static typename std::enable_if<!std::is_same<Container, std::forward_list<typename Container::value_type>>::value, void>::type
        print_size(const Container& c, std::ostream& out) {
            out << "(size: " << c.size() << ") " << std::endl;
        }

        // Helper function for std::forward_list (no size() member)
        template <typename Container>
        static typename std::enable_if<std::is_same<Container, std::forward_list<typename Container::value_type>>::value, void>::type
        print_size(const Container& c, std::ostream& out) {
            out << "(size: " << std::distance(c.begin(), c.end()) << ") " << std::endl;
        }
    #endif

        // Print container information (print size if .size() method exists)
        template <typename Container, typename SourceLocation = void>
        static void print_info(const Container& c, bool show_info, std::ostream& out, SourceLocation loc = {}) {
    #if __cplusplus >= 202002L
            // If C++20 is supported, use source_location to print file and line
            if (show_info) {
                out << "[File: " << loc.file_name() << ", Line: " << loc.line() << "] ";  // Use file_name() to get file path
                if constexpr (has_size_method<Container>::value) {
                    out << "(size: " << c.size() << ") " << std::endl;
                }
            }
    #elif __cplusplus >= 201703L
            // If C++20 is not supported, do not print file and line
            if (show_info) {
                if constexpr (has_size_method<Container>::value) {
                    out << "(size: " << c.size() << ") " << std::endl;
                }
            } 
    #else 
            if (show_info) {
                ContainerPrinter::print_size(c, out);
            }
    #endif
        }

        // Print container (forward_list, list, vector)
        template <typename Container, typename SourceLocation = void>
        static void print_internal(const Container& c, bool show_info, std::ostream& out, SourceLocation loc = {}) {
            print_info(c, show_info, out, loc);
            out << "{";
            bool first = true;
            for (const auto& element : c) {
                if (!first) out << ", ";
                out << element;
                first = false;
            }
            out << "}" << std::endl;
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
        static void print_map_or_unordered_map(const K& key, const V& value, bool is_first_element = true, int indent_level = 0, std::ostream& out = std::cout, SourceLocation loc = {}) {
            if (!is_first_element) {
                out << ",\n";
            }
            out << std::string(indent_level * 4, ' ') << "\"" << key << "\": ";

            // TODO: 这里省略了constexpr会导致 C++11 和 C++14报错
            if constexpr (is_map<V>::value) {  // If value is map or unordered_map, recursively process
                out << "{\n";
                bool first = true;
                for (const auto& [nested_key, nested_value] : value) {
                    ContainerPrinter::print_map_or_unordered_map(nested_key, nested_value, first, indent_level + 1, out, loc);
                    first = false;
                }
                out << "\n" << std::string(indent_level * 4, ' ') << "}";
            } else {  // Regular type
                out << "\"" << value << "\"";
            }
        }

        // Print map
        template <typename K, typename V, typename SourceLocation = void>
        static void print_internal(const std::map<K, V>& m, bool show_info, std::ostream& out, SourceLocation loc = {}) {
            ContainerPrinter::print_info(m, show_info, out, loc);
            out << "{\n";
            bool first = true;
            for (const auto& [key, value] : m) {
                ContainerPrinter::print_map_or_unordered_map(key, value, first, 1, out, loc);
                first = false;
            }
            out << "\n}" << std::endl;
        }

        // Print unordered_map
        template <typename K, typename V, typename SourceLocation = void>
        static void print_internal(const std::unordered_map<K, V>& m, bool show_info, std::ostream& out, SourceLocation loc = {}) {
            ContainerPrinter::print_info(m, show_info, out, loc);
            out << "{\n";
            bool first = true;
            for (const auto& [key, value] : m) {
                ContainerPrinter::print_map_or_unordered_map(key, value, first, 1, out, loc);
                first = false;
            }
            out << "\n}" << std::endl;
        }

        
    public:
        // Unified print function, automatically capture file and line numbers
        template <typename Container>
    #if __cplusplus >= 202002L
        static void print(const Container& c, std::ostream& out = std::cout, bool show_info = true, std::source_location loc = std::source_location::current())
    #else
        static void print(const Container& c, std::ostream& out = std::cout, bool show_info = true, no_source_location loc = no_source_location())
    #endif
        {
            std::lock_guard<std::mutex> guard(ContainerPrinter::print_mutex); 
            ContainerPrinter::print_internal(c, show_info, out, loc);
        }
    };

    // 锁需要在类外定义
    std::mutex ContainerPrinter::print_mutex;
}


#endif