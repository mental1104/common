cmake_minimum_required(VERSION 3.15)
project(experimental_c_hello C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(exp_hello STATIC src/hello.c)
target_include_directories(exp_hello PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(hello_example src/main.c)
target_link_libraries(hello_example PRIVATE exp_hello)

enable_testing()
add_executable(hello_test tests/hello_test.c)
target_link_libraries(hello_test PRIVATE exp_hello)
add_test(NAME hello_test COMMAND hello_test)
