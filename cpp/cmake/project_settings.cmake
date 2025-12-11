set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # 静态库(.a/.lib)输出目录统一放到 build/lib；CMAKE_BINARY_DIR 由配置时的构建目录决定（如 `cmake -S . -B build` 则为 build），此处未显式设置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # 共享库(.so/.dll)输出目录统一放到 build/lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # 可执行文件输出目录统一放到 build/bin

# 打开 compile_commands.json 导出，便于 clangd/clang-tidy 等 IDE 工具获取编译命令
# VS Code 指向方式示例：
#   1) C/C++ 扩展 settings.json: "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json"
#   2) clangd: "clangd.arguments": ["--compile-commands-dir=build"]
# 设置好后即可获得精准跳转/补全/诊断
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CMAKE_BUILD_TYPE 是 CMake 内建缓存变量，单配置生成器下用来选 Debug/Release/RelWithDebInfo/Minsizerel；
# 默认空值，常通过 -DCMAKE_BUILD_TYPE=Release 传入，故这里给未设置时兜底为 Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 指定希望使用 C++20；如果将 REQUIRED 设为 OFF（默认），
# CMake 会在编译器不支持 C++20 时自动降级为其能支持的最高标准，
# 而不会报错；ON 则表示必须满足所指定标准，否则配置失败。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "CXX_STANDARD = ${CMAKE_CXX_STANDARD}")

# 不同构建类型的默认附加编译参数：
#   -Wall -Wextra：打开常用告警，便于发现问题
#   -g：生成调试符号，便于调试/诊断
#   -O2：常用优化级别，性能与编译时间折中，行为更可预期；未选 -O3 以避免过度内联/向量化导致调试困难或潜在 UB 暴露差异
#   -Os：为最小体积优化
#   -DNDEBUG：禁用断言与 debug 分支
# Release 与 RelWithDebInfo 仅差一个 -g，都是 -O2+DNDEBUG；想兼顾优化和调试符号时用 RelWithDebInfo。
# 跑覆盖率时若继续使用 -O2/-DNDEBUG，编译器会删除/折叠分支，导致 gcov/llvm-cov 统计缺失甚至出错，通常需改用低优化并显式加 --coverage。
if(MSVC)
  set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} /W4")
  set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /O2 /Zi /DNDEBUG")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} /Os /DNDEBUG")
else()
  set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -g")
  set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} -Os -DNDEBUG")
endif()

# 将项目头文件根目录添加到全局 include 搜索路径
# （以 INTERFACE/target_include_directories 为佳，但这里全局添加可快速让所有 target 可见）
include_directories(${PROJECT_SOURCE_DIR}/include)

option(AUTO_FETCH_SUBMODULES "Auto init/update git submodules on configure" ON)
# 如开启（默认 ON），在 configure 阶段自动执行 git submodule update --init --recursive --depth=1
if (AUTO_FETCH_SUBMODULES)
  find_package(Git QUIET)
  if (GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.gitmodules") # CMAKE_SOURCE_DIR 为顶层源目录（配置时的 -S），用来定位 .gitmodules
    # message(STATUS ...) 打印配置阶段的普通信息，默认以 `--` 前缀展示，纯提示不影响流程；
    # 与 WARNING/ERROR/FATAL_ERROR 不同，它不会中断或标记失败
    message(STATUS "Auto fetching submodules (git submodule update --init --recursive --depth=1)")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --depth=1
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}  # 在源码根目录执行命令
      RESULT_VARIABLE _subm_res              # 捕获返回码到变量 _subm_res
      OUTPUT_QUIET                           # 忽略标准输出
      ERROR_QUIET                            # 忽略标准错误
    )
    if (NOT _subm_res EQUAL 0) # 逻辑非 + 比较：返回码非 0 视为失败；常见逻辑运算：AND/OR/NOT，比较：EQUAL/LESS/GREATER/STRLESS/STREQUAL/EXISTS 等
      message(WARNING "git submodule update failed; continue with existing content.")
    endif()
  endif()
endif()

# Homebrew toolchain paths (macOS arm64 runners place libs under /opt/homebrew)
if(APPLE)
  if(EXISTS "/opt/homebrew/include")
    include_directories("/opt/homebrew/include")
  endif()
  if(EXISTS "/opt/homebrew/lib")
    link_directories("/opt/homebrew/lib")
  endif()
endif()
