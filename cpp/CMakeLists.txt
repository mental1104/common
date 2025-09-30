cmake_minimum_required(VERSION 3.10)
project(mental1104)

# 设置编译输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 设置 C++ 标准版本
set(CMAKE_CXX_STANDARD 20)  # 可以是 11, 14, 17, 20, 23 等
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 确保使用指定的标准版本

message(CXX_STANDARD " ${CMAKE_CXX_STANDARD}")

# 设置编译选项
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -Os -DNDEBUG")

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 包含 googletest 和 googlemock 的头文件目录
add_subdirectory(thirdparty/googletest)

include_directories(${gtest_SOURCE_DIR}/include ${gmock_SOURCE_DIR}/include)

# ---- BEGIN: scan cpp/lib built thirdparty and add include/lib dirs ----
set(THIRDROOT "${CMAKE_SOURCE_DIR}/lib")
message(STATUS "THIRDROOT = ${THIRDROOT}")

set(THIRD_INCLUDE_DIRS "")
set(THIRD_LIB_DIRS "")

if(EXISTS ${THIRDROOT})
    # 找到各库的 build/include（包含二级目录）
    file(GLOB THIRD_INC_CANDIDATES RELATIVE ${CMAKE_SOURCE_DIR} "${THIRDROOT}/*/build/include" "${THIRDROOT}/*/build/include/*")
    # ---- locate redis++ and hiredis library files and set rpath ----
    # THIRDROOT assumed to be "${CMAKE_SOURCE_DIR}/lib"
    set(REDISPP_LIB_PATH "")
    set(HIREDIS_LIB_PATH "")
    set(THIRD_LIB_DIRS_FOR_RPATH "")

    # try redis-plus-plus lib (.so or .a)
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/redis-plus-plus/build")
        file(GLOB REDISPP_CANDIDATES
            "${CMAKE_SOURCE_DIR}/lib/redis-plus-plus/build/lib/libredis++.so"
            "${CMAKE_SOURCE_DIR}/lib/redis-plus-plus/build/lib/libredis++.so.*"
            "${CMAKE_SOURCE_DIR}/lib/redis-plus-plus/build/lib/libredis++.a"
        )
        if(REDISPP_CANDIDATES)
            list(GET REDISPP_CANDIDATES 0 REDISPP_LIB_PATH)
            message(STATUS "Found redis++ lib: ${REDISPP_LIB_PATH}")
            list(APPEND THIRD_LIB_DIRS_FOR_RPATH "${CMAKE_SOURCE_DIR}/lib/redis-plus-plus/build/lib")
        endif()
    endif()

    # try hiredis lib (.so or .a)
    if(EXISTS "${CMAKE_SOURCE_DIR}/lib/hiredis/build")
        file(GLOB HIREDIS_CANDIDATES
            "${CMAKE_SOURCE_DIR}/lib/hiredis/build/lib/libhiredis.so"
            "${CMAKE_SOURCE_DIR}/lib/hiredis/build/lib/*.a"
        )
        if(HIREDIS_CANDIDATES)
            list(GET HIREDIS_CANDIDATES 0 HIREDIS_LIB_PATH)
            message(STATUS "Found hiredis lib: ${HIREDIS_LIB_PATH}")
            list(APPEND THIRD_LIB_DIRS_FOR_RPATH "${CMAKE_SOURCE_DIR}/lib/hiredis/build/lib")
        endif()
    endif()

    # set RPATH so built executables know where to find shared libs at runtime (relative or absolute)
    if(THIRD_LIB_DIRS_FOR_RPATH)
        # join with semicolon for CMake list -> string
        string(REPLACE ";" ":" THIRD_RPATH_STR "${THIRD_LIB_DIRS_FOR_RPATH}")
        message(STATUS "Setting CMAKE_INSTALL_RPATH to: ${THIRD_RPATH_STR}")
        set(CMAKE_INSTALL_RPATH "${THIRD_RPATH_STR}")
        # also for immediate build/run:
        set(CMAKE_BUILD_RPATH "${THIRD_RPATH_STR}")
    endif()
    # ---- end locate redis/hiredis ----

    foreach(increl ${THIRD_INC_CANDIDATES})
        set(incdir "${CMAKE_SOURCE_DIR}/${increl}")
        if(EXISTS ${incdir})
            list(APPEND THIRD_INCLUDE_DIRS ${incdir})
            message(STATUS "Found thirdparty include: ${incdir}")
        endif()
    endforeach()

    # 找到各库的 build/lib
    file(GLOB THIRD_LIB_CANDIDATES RELATIVE ${CMAKE_SOURCE_DIR} "${THIRDROOT}/*/build/lib")
    foreach(librel ${THIRD_LIB_CANDIDATES})
        set(libdir "${CMAKE_SOURCE_DIR}/${librel}")
        if(EXISTS ${libdir})
            list(APPEND THIRD_LIB_DIRS ${libdir})
            message(STATUS "Found thirdparty lib dir: ${libdir}")
        endif()
    endforeach()
endif()

# 把 include 目录加入全局（确保在 add_executable 之前执行）
if(THIRD_INCLUDE_DIRS)
    include_directories(${THIRD_INCLUDE_DIRS})
endif()

# 作为备选，记录 lib 目录（会用于 link_directories 以及后续寻找具体 .so 路径）
if(THIRD_LIB_DIRS)
    link_directories(${THIRD_LIB_DIRS})
endif()
# ---- END: scan cpp/lib ----



# 安装头文件
install(DIRECTORY include/mental1104
        DESTINATION include)  # 安装到 /usr/local/include

# 安装目标库
# install(TARGETS mental1104
#         DESTINATION lib)  # 安装到 /usr/local/lib

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
    # 链接 gtest, gmock 和 pthread
    # prepare list of libraries to link for this test
    set(EXTRA_LIBS "")
    if(REDISPP_LIB_PATH)
        list(APPEND EXTRA_LIBS ${REDISPP_LIB_PATH})
    endif()
    if(HIREDIS_LIB_PATH)
        list(APPEND EXTRA_LIBS ${HIREDIS_LIB_PATH})
    endif()
    # keep gmp/mpfr/system libs (CMake will find them normally if installed)
    list(APPEND EXTRA_LIBS gmp mpfr gtest gmock gtest_main pthread)

    # link using absolute paths for redis/hiredis when available
    target_link_libraries(${TEST_NAME} PRIVATE ${EXTRA_LIBS})
    # target_link_libraries(${TEST_NAME} PRIVATE hiredis redis++ gmp mpfr gtest gmock gtest_main pthread)
    # 将可执行文件添加为一个 CTest 测试
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# coverage option
OPTION (ENABLE_COVERAGE "Use gcov" ON)
MESSAGE(STATUS ENABLE_COVERAGE=${ENABLE_COVERAGE})
IF(ENABLE_COVERAGE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
ENDIF()

# 启用覆盖率选项
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    option(COVERAGE "Enable coverage reporting" ON)
    if(COVERAGE)
        message(STATUS "Building with coverage support")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
    endif()
endif()

if(COVERAGE)
    find_program(LCOV_EXEC lcov REQUIRED)
    find_program(GENHTML_EXEC genhtml REQUIRED)
    if(LCOV_EXEC AND GENHTML_EXEC)
        add_custom_target(coverage
            COMMAND ${LCOV_EXEC} --directory ${CMAKE_BINARY_DIR} --capture --ignore-errors inconsistent --output-file coverage.info
            COMMAND ${LCOV_EXEC} --ignore-errors unused --remove coverage.info '*/test/*' '/usr/*' '*/external/*' '*/gtest/*' --output-file coverage_filtered.info
            COMMAND ${GENHTML_EXEC} coverage_filtered.info --output-directory ${CMAKE_BINARY_DIR}/coverage_report
            COMMENT "Generating coverage report..."
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        message(WARNING "lcov or genhtml not found, coverage target will not work")
    endif()
endif()