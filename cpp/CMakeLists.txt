cmake_minimum_required(VERSION 3.10)
project(mental1104)

# 设置编译输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 查找 src 中的所有源文件
file(GLOB SOURCES "src/*.cpp")

# 创建静态库或动态库
add_library(mental1104 SHARED ${SOURCES})  # STATIC 可以替换为 SHARED 创建动态库

# 安装头文件
install(DIRECTORY include/mental1104
        DESTINATION include)  # 安装到 /usr/local/include

# 安装目标库
install(TARGETS mental1104
        DESTINATION lib)  # 安装到 /usr/local/lib




# 添加测试可执行文件
# add_executable(test_example tests/test_example.cpp)
# target_link_libraries(test_example my_library)  # 链接库到测试