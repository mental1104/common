#ifndef MENTAL1104_META_FORMAT_SUPPORT_H
#define MENTAL1104_META_FORMAT_SUPPORT_H

#include "compiler_support.h"

// Detect and normalize <format> support; kept separate to avoid repeating
// feature checks at each use site。 也可合并回
// compiler_support.h，但独立文件能让不需要 <format>
// 的编译单元避免额外的探测/包含。
#if M1104_HAS_CXX20 && M1104_HAS_INCLUDE(<format>)
#include <format>
#ifndef __cpp_lib_format
#define __cpp_lib_format                                                       \
  201907L // 某些旧版 libstdc++/libc++ 尽管提供 <format>
          // 却缺失特征宏；缺失会导致后续判定回退，这里填入标准值 201907L
          // 视为完整支持；参考特征宏表
          // https://en.cppreference.com/w/cpp/feature_test
#endif
#endif

#if M1104_HAS_CXX20 && defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#define M1104_HAS_STD_FORMAT 1
#else
#define M1104_HAS_STD_FORMAT 0
#endif

#endif // MENTAL1104_META_FORMAT_SUPPORT_H
