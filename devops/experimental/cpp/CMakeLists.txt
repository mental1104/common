cmake_minimum_required(VERSION 3.15)
project(experimental_cpp_hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(exp_hello STATIC src/hello.cpp)
target_include_directories(exp_hello PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(hello_example src/main.cpp)
target_link_libraries(hello_example PRIVATE exp_hello)

enable_testing()
add_executable(hello_test tests/hello_test.cpp)
target_link_libraries(hello_test PRIVATE exp_hello)
add_test(NAME hello_test COMMAND hello_test)
