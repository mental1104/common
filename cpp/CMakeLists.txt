cmake_minimum_required(VERSION 3.10)
project(mental1104)

# 设置编译输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 查找 src 中的所有源文件
# file(GLOB SOURCES "src/*.cpp")

# 创建静态库或动态库
# add_library(mental1104 SHARED ${SOURCES})  # STATIC 可以替换为 SHARED 创建动态库

# 安装头文件
install(DIRECTORY include/mental1104
        DESTINATION include)  # 安装到 /usr/local/include

# 安装目标库
# install(TARGETS mental1104
#         DESTINATION lib)  # 安装到 /usr/local/lib

# 添加测试可执行文件
# add_executable(test_example tests/test_example.cpp)
# target_link_libraries(test_example my_library)  # 链接库到测试

# 3. 设置测试目录
enable_testing()

# 设置测试目录
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# 查找 test 目录下的所有 cpp 测试文件
file(GLOB TEST_FILES "${TEST_DIR}/*.cpp")

# 遍历每个测试文件
foreach(TEST_FILE ${TEST_FILES})
    # 获取测试文件名（不包含路径和后缀）
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    # 为每个测试文件创建独立的可执行文件
    add_executable(${TEST_NAME} ${TEST_FILE})
    # 链接所需的库（例如 GTest 或其他库）
    target_link_libraries(${TEST_NAME} PRIVATE mental1104 gtest gtest_main pthread)
    # 将可执行文件添加为一个 CTest 测试
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()