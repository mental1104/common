#ifndef __MENTAL1104_TIMED
#define __MENTAL1104_TIMED

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <typeinfo>

namespace mental1104 {
static std::mutex time_mutex;
// 主模板声明
template <typename T>
class Timed;

// 针对函数类型 T = R(Args...) 的部分特化
template <typename R, typename... Args>
class Timed<R(Args...)> {
   public:
    R operator()(Args... args) {
        std::cout << "Entering " << name << '\n';
        auto start = std::chrono::high_resolution_clock::now();
        R result = func(args...);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << "Exiting " << name << " with " << std::fixed
                  << std::setprecision(10) << duration.count() << " seconds"
                  << std::endl;
        ;
        return result;
    }

   private:
    Timed(std::function<R(Args...)> func, std::string name)
        : func{std::move(func)}, name{std::move(name)} {}
    std::function<R(Args...)> func;
    std::string name;

    template <typename U, typename... V>
    friend Timed<U(V...)> make_timed(U (*func)(V...), const std::string& name);
};

// 针对返回类型为void的函数进行部分特化
template <typename... Args>
class Timed<void(Args...)> {
   public:
    void operator()(Args... args) {
        std::cout << "Entering " << name << '\n';
        auto start = std::chrono::high_resolution_clock::now();
        func(args...);  // 不需要保存返回值
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << "Exiting "
                  << (name.size() == 0 ? "with " : name + " with ")
                  << std::fixed << std::setprecision(10) << duration.count()
                  << " seconds" << std::endl;
    }

   private:
    Timed(std::function<void(Args...)> func, std::string name)
        : func{std::move(func)}, name{std::move(name)} {}

    std::function<void(Args...)> func;
    std::string name;

    template <typename... V>
    friend Timed<void(V...)> make_timed(void (*func)(V...),
                                        const std::string& name);
};

template <typename R, typename... Args>
Timed<R(Args...)> make_timed(R (*func)(Args...),
                             const std::string& name = std::string()) {
    std::lock_guard<std::mutex> guard(time_mutex);
    return Timed<R(Args...)>(std::function<R(Args...)>(func), name);
}

// 针对返回类型为void的函数的辅助函数
template <typename... Args>
Timed<void(Args...)> make_timed(void (*func)(Args...),
                                const std::string& name = std::string()) {
    std::lock_guard<std::mutex> guard(time_mutex);
    return Timed<void(Args...)>(std::function<void(Args...)>(func), name);
}
}  // namespace mental1104

#endif