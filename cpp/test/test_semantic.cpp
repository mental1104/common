#include <gtest/gtest.h>
#include <type_traits>
#include <vector>
#include <string>
#include "mental1104/semantic.h"

using namespace mental1104;

struct S1 : Movable {  // 仅可移动
    int x = 0;
};

struct S2 : NonCopyable { // 成员有资源，沿用“仅可移动”语义
    std::string s;
};

struct S3 : Immovable {   // 完全不允许移动/拷贝
    int x = 0;
};

TEST(Semantics, Traits) {
    static_assert(!std::is_copy_constructible_v<S1>);
    static_assert(std::is_move_constructible_v<S1>);
    static_assert(std::is_nothrow_move_constructible_v<S1>);
    static_assert(is_move_only_v<S1>);
    static_assert(sizeof(S1) == sizeof(int), "EBO 应不增大对象尺寸");

    static_assert(!std::is_copy_assignable_v<S2>);
    static_assert(std::is_move_assignable_v<S2>);
    static_assert(std::is_move_constructible_v<S2>);

    static_assert(!std::is_move_constructible_v<S3>);
    static_assert(!std::is_copy_constructible_v<S3>);
}

TEST(Semantics, MoveTransfersState) {
    S2 a; a.s = "hello";
    S2 b = std::move(a);
    EXPECT_EQ(b.s, "hello");
}

TEST(Semantics, VectorPrefersNoexceptMove) {
    static_assert(std::is_nothrow_move_constructible_v<S2>);
    std::vector<S2> v;
    v.reserve(2);
    S2 x; x.s = "x";
    v.push_back(std::move(x));
    S2 y; y.s = "y";
    v.push_back(std::move(y));
    EXPECT_EQ(v.size(), 2u);
}

TEST(Semantics, MacroMoveOnly) {
    struct S4 {
        std::string s;
        MENTAL1104_MOVE_ONLY(S4)
    };
    static_assert(!std::is_copy_constructible_v<S4>);
    static_assert(std::is_move_constructible_v<S4>);
}
