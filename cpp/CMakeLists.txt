cmake_minimum_required(VERSION 3.10)
project(mental1104)

# ===== 基础配置（保留原逻辑） =====
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "CXX_STANDARD = ${CMAKE_CXX_STANDARD}")

set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -g")
set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} -Os -DNDEBUG")

include_directories(${PROJECT_SOURCE_DIR}/include)

# ===== 可选：配置阶段尝试拉取 submodules（失败不阻断） =====
option(AUTO_FETCH_SUBMODULES "Auto init/update git submodules on configure" ON)
if (AUTO_FETCH_SUBMODULES)
	find_package(Git QUIET)
	if (GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.gitmodules")
		message(STATUS "Auto fetching submodules (git submodule update --init --recursive --depth=1)")
		execute_process(
			COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --depth=1
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			RESULT_VARIABLE _subm_res
			OUTPUT_QUIET
			ERROR_QUIET
		)
		if (NOT _subm_res EQUAL 0)
			message(WARNING "git submodule update failed; continue with existing content.")
		endif()
	endif()
endif()

# ===== gtest（用目标名链接） =====
add_subdirectory(thirdparty/googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gmock_SOURCE_DIR}/include)
set(GTEST_LINK gtest gtest_main)

# ===== 原有第三方扫描（build/include & build/lib） =====
set(THIRDROOT "${CMAKE_SOURCE_DIR}/lib")
message(STATUS "THIRDROOT = ${THIRDROOT}")

set(THIRD_INCLUDE_DIRS "")
set(THIRD_LIB_DIRS "")

if(EXISTS ${THIRDROOT})
	file(GLOB THIRD_INC_CANDIDATES RELATIVE ${CMAKE_SOURCE_DIR}
		"${THIRDROOT}/*/build/include"
		"${THIRDROOT}/*/build/include/*"
	)
	foreach(increl ${THIRD_INC_CANDIDATES})
		set(incdir "${CMAKE_SOURCE_DIR}/${increl}")
		if(EXISTS ${incdir})
			list(APPEND THIRD_INCLUDE_DIRS ${incdir})
			message(STATUS "Found thirdparty include: ${incdir}")
		endif()
	endforeach()

	file(GLOB THIRD_LIB_CANDIDATES RELATIVE ${CMAKE_SOURCE_DIR}
		"${THIRDROOT}/*/build/lib"
	)
	foreach(librel ${THIRD_LIB_CANDIDATES})
		set(libdir "${CMAKE_SOURCE_DIR}/${librel}")
		if(EXISTS ${libdir})
			list(APPEND THIRD_LIB_DIRS ${libdir})
			message(STATUS "Found thirdparty lib dir: ${libdir}")
		endif()
	endforeach()
endif()

# —— 避免命中 redis++ 的 build/include 半成品头
list(FILTER THIRD_INCLUDE_DIRS EXCLUDE REGEX ".*/lib/redis-plus-plus/build/include(/.*)?$")

if(THIRD_INCLUDE_DIRS)
	include_directories(${THIRD_INCLUDE_DIRS})
endif()
if(THIRD_LIB_DIRS)
	link_directories(${THIRD_LIB_DIRS})
endif()

# ====================================================================
# 高扩展的第三方注册：生成导入目标
#   <NAME>::headers  (INTERFACE, 仅携带 include)
#   <NAME>::lib      (IMPORTED,  携带库与 include；仅在找到库文件时创建)
#
# 用法：
#   register_component(NAME SUBPATH LIB_GLOB INC_REL <rel1> <rel2> ...)
# 导出：
#   HAVE_<NAME>       ：SUBPATH 下是否有 CMakeLists.txt
#   <NAME>_INC_DIRS   ：用于聚合 include 的列表
#   目标 <NAME>::headers / <NAME>::lib
# ====================================================================
include(CMakeParseArguments)
set_property(GLOBAL PROPERTY COMPONENT_RPATH_DIRS "")

function(register_component NAME SUBPATH)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs INC_REL LIB_GLOBS)
    cmake_parse_arguments(CMP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(_root "${CMAKE_SOURCE_DIR}/${SUBPATH}")

    # 存在性：仅用于“可用与否”判断
    if (EXISTS "${_root}/CMakeLists.txt")
        set(HAVE_${NAME} TRUE PARENT_SCOPE)
        message(STATUS "Component ${NAME} = AVAILABLE at ${SUBPATH}")
    else()
        set(HAVE_${NAME} FALSE PARENT_SCOPE)
        message(STATUS "Component ${NAME} = MISSING at ${SUBPATH} (no CMakeLists.txt)")
    endif()

    # include 列表
    set(_inc_abs "")
    foreach(rel ${CMP_INC_REL})
        if (EXISTS "${_root}/${rel}")
            list(APPEND _inc_abs "${_root}/${rel}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _inc_abs)
    set(${NAME}_INC_DIRS "${_inc_abs}" PARENT_SCOPE)

    # INTERFACE 头目标
    set(_hdr_tgt "${NAME}::headers")
    if (NOT TARGET ${_hdr_tgt})
        add_library(${_hdr_tgt} INTERFACE IMPORTED)
    endif()
    set_target_properties(${_hdr_tgt} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_inc_abs}"
    )
    message(STATUS "${_hdr_tgt}.INCLUDES = ${_inc_abs}")

    # 在 build/ build/lib build/src 下用多模式找库
    set(_lib_path "")
    if (EXISTS "${_root}/build")
        foreach(d "" "lib" "src")
            foreach(pat IN LISTS CMP_LIB_GLOBS)
                file(GLOB _cands "${_root}/build/${d}/${pat}")
                if (_cands)
                    # 拿第一个命中
                    list(GET _cands 0 _lib_path)
                    break()
                endif()
            endforeach()
            if (_lib_path)
                break()
            endif()
        endforeach()
    endif()

    if (_lib_path)
        set(_lib_tgt "${NAME}::lib")
        if (NOT TARGET ${_lib_tgt})
            add_library(${_lib_tgt} UNKNOWN IMPORTED)
        endif()
        set_target_properties(${_lib_tgt} PROPERTIES
            IMPORTED_LOCATION "${_lib_path}"
            INTERFACE_INCLUDE_DIRECTORIES "${_inc_abs}"
        )
        get_filename_component(_libdir "${_lib_path}" DIRECTORY)
        set_property(GLOBAL APPEND PROPERTY COMPONENT_RPATH_DIRS "${_libdir}")

        set(HAVE_LIB_${NAME} TRUE PARENT_SCOPE)
        message(STATUS "Found ${NAME} lib: ${_lib_path}")
    else()
        set(HAVE_LIB_${NAME} FALSE PARENT_SCOPE)
    endif()
endfunction()


# hiredis：会生成 libhiredis.so / libhiredisd.so / .a / .dylib 等
register_component(HIREDIS "lib/hiredis"
    LIB_GLOBS "libhiredis*.so*" "libhiredis*.a" "libhiredis*.dylib"
    INC_REL .
)

# redis-plus-plus：也放宽以兼容 debug 后缀与多平台
register_component(REDISPP "lib/redis-plus-plus"
    LIB_GLOBS "libredis++*.so*" "libredis++*.a" "libredis++*.dylib"
    INC_REL src
)

# ====== TEMP PATCH (BEGIN) ———— redis-plus-plus 缺失 cxx_utils.h 的临时修复 ======
# 说明：
#   部分版本/配置下，redis-plus-plus 没有统一头 "sw/redis++/cxx_utils.h"，
#   但 utils.h 会引用它。官方通常在 install 时才把 cxx11/cxx17 变体“展平”。
#   这里在构建目录生成一个 overlay/cxx_utils.h，用 __has_include 多路径探测。
#   —— 不改子模块源码；未来上游修复后，可删除本段以及调用行，或用 -DRPP_SHIM_ENABLE=OFF 关闭。

option(RPP_SHIM_ENABLE "Enable redis++ cxx_utils.h overlay shim" ON)

# 把 include 目录列表同步到导入目标（辅助函数）
function(_rpp_propagate_inc tgt incs)
	if (TARGET ${tgt})
		get_target_property(_old_inc ${tgt} INTERFACE_INCLUDE_DIRECTORIES)
		if (NOT _old_inc)
			set(_old_inc "")
		endif()
		list(PREPEND _old_inc ${incs})
		list(REMOVE_DUPLICATES _old_inc)
		set_target_properties(${tgt} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_old_inc}")
	endif()
endfunction()

# 临时补丁：为 redis-plus-plus 注入 cxx_utils.h / tls.h 聚合头
# 目标：
#  - 提前放置到 build/overlay/redis-plus-plus/sw/redis++/*
#  - 包含上游版本时直接转发；若不存在，提供最小可编译的兜底实现
#  - BEFORE 方式插入 include 路径，确保优先命中 overlay
# 临时补丁：为 redis-plus-plus 注入 cxx_utils.h / tls.h 聚合头
function(rpp_apply_cxx_utils_shim)
    set(_rpp_root      "${CMAKE_SOURCE_DIR}/lib/redis-plus-plus")
    set(_overlay_root  "${CMAKE_BINARY_DIR}/overlay/redis-plus-plus")
    set(_overlay_inc   "${_overlay_root}")
    file(MAKE_DIRECTORY "${_overlay_inc}/sw/redis++")

    # ---- cxx_utils.h：补全 StringView/Optional 系列 ----
    set(_cxx_utils "${_overlay_inc}/sw/redis++/cxx_utils.h")
    file(WRITE "${_cxx_utils}" [=[
#pragma once
// Overlay adapter for redis++ cxx_utils.h (temporary shim)

#if __has_include(<sw/redis++/cxx17/cxx_utils.h>)
#  include <sw/redis++/cxx17/cxx_utils.h>
#elif __has_include(<sw/redis++/cxx11/cxx_utils.h>)
#  include <sw/redis++/cxx11/cxx_utils.h>
#else
#  include <string>
#  include <string_view>
#  include <optional>
#  include <utility>
#  include <tuple>

namespace sw { namespace redis {
using StringView = std::string_view;
template <typename T> using Optional = std::optional<T>;
using OptionalString      = Optional<std::string>;
using OptionalLongLong    = Optional<long long>;
using OptionalDouble      = Optional<double>;
using OptionalStringPair  = Optional<std::pair<std::string, std::string>>;
}} // namespace sw::redis
#endif
]=])
    message(STATUS "[RPP SHIM] generated: ${_cxx_utils}")

    # ---- tls.h：首选上游；都没有则提供可默认构造的桩 ----
    set(_tls "${_overlay_inc}/sw/redis++/tls.h")
    file(WRITE "${_tls}" [=[
#pragma once
// Overlay adapter for redis++ tls.h (temporary shim)

#if __has_include(<sw/redis++/tls/tls.h>)
#  include <sw/redis++/tls/tls.h>
#elif __has_include(<sw/redis++/no_tls/tls.h>)
#  include <sw/redis++/no_tls/tls.h>
#else
#  include <memory>
#  include <cstddef>

namespace sw { namespace redis { namespace tls {

// 与上游接口对齐的最小桩：
// 1) TlsOptions：占位
// 2) NullDeleter：可默认构造，用于 unique_ptr 默认构造
// 3) TlsContextUPtr：使用可默认构造的 deleter，避免编译期约束失败
struct TlsOptions {};

// 可默认构造 / 可拷贝的小型 deleter
struct NullDeleter {
    void operator()(void* /*p*/) const noexcept {}
};

// 注意：必须可默认构造，Connection 默认成员初始化会用到
using TlsContextUPtr = std::unique_ptr<void, NullDeleter>;

}}} // namespace sw::redis::tls
#endif
]=])
    message(STATUS "[RPP SHIM] generated: ${_tls}")

    # ---- 提前注入 include 路径（BEFORE 确保 overlay 优先）----
    set(_want_includes
        "${_overlay_inc}"
        "${_rpp_root}/src"
        "${_rpp_root}/build/src"
    )
    foreach(_dir IN LISTS _want_includes)
        if (EXISTS "${_dir}")
            include_directories(BEFORE "${_dir}")
            message(STATUS "[RPP SHIM] include (BEFORE): ${_dir}")
        endif()
    endforeach()

    set(RPP_OVERLAY_INCLUDE_DIR "${_overlay_inc}" PARENT_SCOPE)
endfunction()



# 调用临时补丁（若未来不需要，可删除本行与上方 BEGIN/END 整段）
rpp_apply_cxx_utils_shim()
# ====== TEMP PATCH (END) =======================================================

# 设置 RPATH（若找到了任一第三方库目录）
get_property(_RPATHS GLOBAL PROPERTY COMPONENT_RPATH_DIRS)
if (_RPATHS)
	list(REMOVE_DUPLICATES _RPATHS)
	string(REPLACE ";" ":" THIRD_RPATH_STR "${_RPATHS}")
	message(STATUS "Setting RPATH to: ${THIRD_RPATH_STR}")
	set(CMAKE_INSTALL_RPATH "${THIRD_RPATH_STR}")
	set(CMAKE_BUILD_RPATH   "${THIRD_RPATH_STR}")
endif()

# ===== 安装头文件（保留） =====
install(DIRECTORY include/mental1104 DESTINATION include)

# ===== 测试（按依赖自动决定是否编译） =====
enable_testing()

set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
file(GLOB TEST_FILES "${TEST_DIR}/*.cpp")
list(LENGTH TEST_FILES _TEST_COUNT)
message(STATUS "Collected ${_TEST_COUNT} test files under ${TEST_DIR}")

# 依赖声明：
# - TEST_DEPS_*            ：仅需头可见（会链接 <NAME>::headers）
# - TEST_REQUIRE_LIBS_*    ：还要求库已构建（会链接 <NAME>::lib）
set(TEST_DEPS_test_redis_lock "HIREDIS;REDISPP")
set(TEST_REQUIRE_LIBS_test_redis_lock "HIREDIS;REDISPP")

function(add_optional_test SRC)
	get_filename_component(TEST_NAME "${SRC}" NAME_WE)
	message(STATUS "Consider test ${TEST_NAME} -> ${SRC}")

	# 二次解引用，稳取依赖列表
	set(_hdr_var  "TEST_DEPS_${TEST_NAME}")
	set(_libs_var "TEST_REQUIRE_LIBS_${TEST_NAME}")
	set(req_headers "${${_hdr_var}}")
	set(req_libs    "${${_libs_var}}")

	# 组件存在性检查
	set(missing "")
	foreach(dep IN LISTS req_headers req_libs)
		if (dep AND NOT HAVE_${dep})
			list(APPEND missing "${dep}")
		endif()
	endforeach()
	# 必须存在库产物（仅对 req_libs）
	foreach(dep IN LISTS req_libs)
		if (dep AND NOT TARGET ${dep}::lib)
			list(APPEND missing "${dep}(lib)")
		endif()
	endforeach()
	if (missing)
		message(STATUS "Skip test ${TEST_NAME}: missing => ${missing}")
		return()
	endif()

	# 聚合依赖 include（置顶）
	set(ALL_DEPS ${req_headers} ${req_libs})
	list(REMOVE_DUPLICATES ALL_DEPS)
	set(AGG_INC "")
	foreach(dep IN LISTS ALL_DEPS)
		if (dep)
			set(_incvar "${dep}_INC_DIRS")
			if (DEFINED ${_incvar})
				list(APPEND AGG_INC ${${_incvar}})
			endif()
		endif()
	endforeach()
	list(REMOVE_DUPLICATES AGG_INC)
	message(STATUS "  -> include dirs (priority): ${AGG_INC}")

	add_executable(${TEST_NAME} "${SRC}")
	if (AGG_INC)
		target_include_directories(${TEST_NAME} BEFORE PRIVATE
			${AGG_INC}
			${PROJECT_SOURCE_DIR}/include
			${THIRD_INCLUDE_DIRS}
		)
	else()
		target_include_directories(${TEST_NAME} PRIVATE
			${PROJECT_SOURCE_DIR}/include
			${THIRD_INCLUDE_DIRS}
		)
	endif()

	foreach(dep IN LISTS req_headers)
		if (dep AND TARGET ${dep}::headers)
			target_link_libraries(${TEST_NAME} PRIVATE ${dep}::headers)
		endif()
	endforeach()
	foreach(dep IN LISTS req_libs)
		if (dep AND TARGET ${dep}::lib)
			target_link_libraries(${TEST_NAME} PRIVATE ${dep}::lib)
		endif()
	endforeach()

	target_link_libraries(${TEST_NAME} PRIVATE
		${GTEST_LINK}
		mpfr gmp
		pthread
	)

	add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
	message(STATUS "Added test ${TEST_NAME}")
endfunction()

foreach(SRC ${TEST_FILES})
	add_optional_test("${SRC}")
endforeach()

# =====（可选）仅对 lib/* 下“有 CMakeLists.txt”的库 add_subdirectory（默认关）=====
option(BUILD_THIRD_LIBS_FROM_SOURCE "Add lib/* that contain CMakeLists.txt via add_subdirectory" OFF)
if (BUILD_THIRD_LIBS_FROM_SOURCE)
	file(GLOB _LIB_SUBS RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/lib/*")
	foreach(_rel ${_LIB_SUBS})
		if (EXISTS "${CMAKE_SOURCE_DIR}/${_rel}/CMakeLists.txt")
			message(STATUS "add_subdirectory(${_rel})")
			add_subdirectory("${CMAKE_SOURCE_DIR}/${_rel}" "${CMAKE_BINARY_DIR}/${_rel}_build" EXCLUDE_FROM_ALL)
		else()
			message(STATUS "Skip lib (${_rel}): no CMakeLists.txt")
		endif()
	endforeach()
endif()

# ===== 覆盖率（保留原逻辑） =====
option(ENABLE_COVERAGE "Use gcov" ON)
message(STATUS "ENABLE_COVERAGE=${ENABLE_COVERAGE}")
if(ENABLE_COVERAGE)
	set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
	set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
	set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	option(COVERAGE "Enable coverage reporting" ON)
	if(COVERAGE)
		message(STATUS "Building with coverage support")
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
		set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
	endif()
endif()

if(COVERAGE)
  find_program(LCOV_EXEC lcov REQUIRED)
  find_program(GENHTML_EXEC genhtml REQUIRED)
  if(LCOV_EXEC AND GENHTML_EXEC)
    add_custom_target(coverage
      COMMAND ${LCOV_EXEC}
              --directory ${CMAKE_BINARY_DIR}
              --capture
              --ignore-errors mismatch,inconsistent
              --rc geninfo_unexecuted_blocks=1
              --output-file coverage.info
      COMMAND ${LCOV_EXEC}
              --remove coverage.info
              '*/test/*' '/usr/*' '*/external/*' '*/gtest/*'
              --ignore-errors unused
              --output-file coverage_filtered.info
      # === 新增：把每个文件的覆盖率打印到命令行（并保存一份文本） ===
      COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C
              ${LCOV_EXEC} --list coverage_filtered.info
              --ignore-errors mismatch,inconsistent
              --rc geninfo_unexecuted_blocks=1
      COMMAND ${GENHTML_EXEC} coverage_filtered.info
              --output-directory ${CMAKE_BINARY_DIR}/coverage_report
      COMMENT "Generating coverage report..."
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
  else()
    message(WARNING "lcov or genhtml not found, coverage target will not work")
  endif()
endif()


