#pragma once
#include <type_traits>

namespace mental1104 {

// 仅禁拷贝，允许 noexcept 移动；不声明析构函数（避免抑制隐式 move）
struct NonCopyable {
protected:
  NonCopyable() = default;

  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;

  NonCopyable(NonCopyable &&) noexcept = default;
  NonCopyable &operator=(NonCopyable &&) noexcept = default;
};

// 语义别名：Move-only（继承 NonCopyable），并显式公开移动成员
struct Movable : NonCopyable {
protected:
  using NonCopyable::NonCopyable; // 禁止直接实例化基类
public:
  Movable() = default;
  Movable(Movable &&) noexcept = default;
  Movable &operator=(Movable &&) noexcept = default;

  Movable(const Movable &) = delete;
  Movable &operator=(const Movable &) = delete;
};

// 完全不可移动不可拷贝（资源绑死场景）
struct Immovable {
protected:
  Immovable() = default;

  Immovable(const Immovable &) = delete;
  Immovable &operator=(const Immovable &) = delete;
  Immovable(Immovable &&) = delete;
  Immovable &operator=(Immovable &&) = delete;
};

// 在类体内一键声明“仅可移动”
#define MENTAL1104_MOVE_ONLY(Class)                                            \
public:                                                                        \
  Class() = default;                                                           \
  Class(Class &&) noexcept = default;                                          \
  Class &operator=(Class &&) noexcept = default;                               \
  Class(const Class &) = delete;                                               \
  Class &operator=(const Class &) = delete;

// 便捷 traits：是否“仅可移动”
template <class T>
inline constexpr bool is_move_only_v =
    !std::is_copy_constructible_v<T> && std::is_move_constructible_v<T>;

} // namespace mental1104
