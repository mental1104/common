#ifndef __MENTAL1104_TIMED
#define __MENTAL1104_TIMED

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <typeinfo>

namespace mental1104 {
namespace detail {
/// 输出计时包装器的进入消息。
/// @param name 调用期间有效的函数显示名称。
void print_timed_enter(const std::string &name);
/// 输出计时包装器的退出消息，保留原固定小数格式。
/// @param label 名称及 with 前缀，不保存引用。
/// @param seconds 本次执行耗时，单位为秒。
void print_timed_exit(const std::string &label, double seconds);
} // namespace detail

static std::mutex time_mutex;
// 主模板声明
template <typename T> class Timed;

// 针对函数类型 T = R(Args...) 的部分特化
/// 包装具有确定参数和返回类型的函数；输出由已编译核心提供。
template <typename R, typename... Args> class Timed<R(Args...)> {
public:
  /// 执行被包装函数并在正常返回时输出耗时，异常继续传播。
  /// @param args 传给被包装函数的参数，沿用既有传值语义。
  /// @return 被包装函数返回的值。
  R operator()(Args... args) {
    detail::print_timed_enter(this->name);
    auto start = std::chrono::high_resolution_clock::now();
    R result = func(args...);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    detail::print_timed_exit(this->name + " with ", duration.count());
    return result;
  }

private:
  Timed(std::function<R(Args...)> func, std::string name)
      : func{std::move(func)}, name{std::move(name)} {}
  std::function<R(Args...)> func;
  std::string name;

  template <typename U, typename... V>
  friend Timed<U(V...)> make_timed(U (*func)(V...), const std::string &name);
};

// 针对返回类型为void的函数进行部分特化
/// 包装无返回值的函数；模板负责调用，库负责计时消息输出。
template <typename... Args> class Timed<void(Args...)> {
public:
  /// 执行被包装函数并在正常返回时输出耗时，异常继续传播。
  /// @param args 传给被包装函数的参数，沿用既有传值语义。
  void operator()(Args... args) {
    detail::print_timed_enter(this->name);
    auto start = std::chrono::high_resolution_clock::now();
    func(args...); // 不需要保存返回值
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    detail::print_timed_exit(
        this->name.empty() ? "with " : this->name + " with ", duration.count());
  }

private:
  Timed(std::function<void(Args...)> func, std::string name)
      : func{std::move(func)}, name{std::move(name)} {}

  std::function<void(Args...)> func;
  std::string name;

  template <typename... V>
  friend Timed<void(V...)> make_timed(void (*func)(V...),
                                      const std::string &name);
};

template <typename R, typename... Args>
Timed<R(Args...)> make_timed(R (*func)(Args...),
                             const std::string &name = std::string()) {
  std::lock_guard<std::mutex> guard(time_mutex);
  return Timed<R(Args...)>(std::function<R(Args...)>(func), name);
}

// 针对返回类型为void的函数的辅助函数
template <typename... Args>
Timed<void(Args...)> make_timed(void (*func)(Args...),
                                const std::string &name = std::string()) {
  std::lock_guard<std::mutex> guard(time_mutex);
  return Timed<void(Args...)>(std::function<void(Args...)>(func), name);
}
} // namespace mental1104

#endif
