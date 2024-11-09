#ifndef MENTAL1104_UTIL
#define MENTAL1104_UTIL

#include <iostream>
#include <functional>
#include <string>
#include <chrono>
#include <iomanip>
#include <typeinfo>


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
}

#endif


