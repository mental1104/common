cmake_minimum_required(VERSION 3.10) # 3.10 对齐 Ubuntu 18.04 默认 CMake，便于兼容老环境；但后文用到的 CONFIGURE_DEPENDS (需更高版本，如 ≥3.12) 和 cxx_std_20 等特性实际需要更新版 CMake
project(mental1104) # 定义项目名/语言作用域，初始化诸如 ${PROJECT_SOURCE_DIR}、${PROJECT_BINARY_DIR} 等变量供后续使用

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake") 
# list(APPEND <list> <元素...>) 语法：向变量 CMAKE_MODULE_PATH 末尾追加当前 CMakeLists 所在目录下的 cmake 子目录，便于 include 自定义 *.cmake；
# CMAKE_MODULE_PATH 初值通常为空（内建变量），但可能已被工具链文件/上层项目预设

include(cmake/project_settings.cmake)
include(cmake/deps.cmake)
include(cmake/core_library.cmake)
include(cmake/stacktrace.cmake)
include(cmake/testing.cmake)
include(cmake/benchmarks.cmake)
include(cmake/coverage.cmake)
