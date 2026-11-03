cmake_minimum_required(VERSION 3.10)
project(common LANGUAGES C CXX)

# 根目录只提供 C++ 构建入口；其余语言继续使用各自的 devtool 命令。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(M1104_ROOT_ENTRY FALSE)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(M1104_ROOT_ENTRY TRUE)
endif()
enable_testing()
add_subdirectory(cpp)
