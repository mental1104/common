cmake_minimum_required(VERSION 3.10)
project(mental1104)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_subdirectory(thirdparty/googletest)

set(BENCH_THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/thirdparty/benchmark")
if (EXISTS "${BENCH_THIRDPARTY_DIR}/CMakeLists.txt")
  set(BENCHMARK_ENABLE_TESTING      OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS  OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL      OFF CACHE BOOL "" FORCE)
  add_subdirectory(thirdparty/benchmark EXCLUDE_FROM_ALL)
  set(HAVE_GOOGLE_BENCHMARK TRUE)
else()
  set(HAVE_GOOGLE_BENCHMARK FALSE)
endif()

include_directories(${gtest_SOURCE_DIR}/include ${gmock_SOURCE_DIR}/include)
set(GTEST_LINK gtest gtest_main)

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

list(FILTER THIRD_INCLUDE_DIRS EXCLUDE REGEX ".*/lib/redis-plus-plus/build/include(/.*)?$")

set(_LIB_HDR_CANDS
  "${THIRDROOT}/boost"
  "${THIRDROOT}/boost/include"
  "${THIRDROOT}/rapidjson/include"
  "${THIRDROOT}/cJSON"
  "${THIRDROOT}/hiredis"
  "${THIRDROOT}/hiredis/include"
  "${THIRDROOT}/pystring"
  "${THIRDROOT}/spdlog/include"
  "${THIRDROOT}/redis-plus-plus/src"
)
foreach(_d ${_LIB_HDR_CANDS})
  if(EXISTS "${_d}")
    list(APPEND THIRD_INCLUDE_DIRS "${_d}")
    message(STATUS "Found thirdparty include: ${_d}")
  endif()
endforeach()

if(THIRD_INCLUDE_DIRS)
	include_directories(BEFORE ${THIRD_INCLUDE_DIRS})
endif()
if(THIRD_LIB_DIRS)
	link_directories(${THIRD_LIB_DIRS})
endif()

include(CMakeParseArguments)
set_property(GLOBAL PROPERTY COMPONENT_RPATH_DIRS "")
set_property(GLOBAL PROPERTY REGISTERED_COMPONENTS "")

function(register_component NAME SUBPATH)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs INC_REL LIB_GLOBS)
    cmake_parse_arguments(CMP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(_root "${CMAKE_SOURCE_DIR}/${SUBPATH}")

    if (EXISTS "${_root}/CMakeLists.txt")
        set(HAVE_${NAME} TRUE PARENT_SCOPE)
        message(STATUS "Component ${NAME} = AVAILABLE at ${SUBPATH}")
    else()
        set(HAVE_${NAME} FALSE PARENT_SCOPE)
        message(STATUS "Component ${NAME} = MISSING at ${SUBPATH} (no CMakeLists.txt)")
    endif()

    set(_inc_abs "")
    foreach(rel ${CMP_INC_REL})
        if (EXISTS "${_root}/${rel}")
            list(APPEND _inc_abs "${_root}/${rel}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _inc_abs)
    set(${NAME}_INC_DIRS "${_inc_abs}" PARENT_SCOPE)

    set(_hdr_tgt "${NAME}::headers")
    if (NOT TARGET ${_hdr_tgt})
        add_library(${_hdr_tgt} INTERFACE IMPORTED)
    endif()
    set_target_properties(${_hdr_tgt} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_inc_abs}"
    )
    message(STATUS "${_hdr_tgt}.INCLUDES = ${_inc_abs}")

    set(_lib_path "")
    if (EXISTS "${_root}/build")
        foreach(d "" "lib" "src")
            foreach(pat IN LISTS CMP_LIB_GLOBS)
                file(GLOB _cands "${_root}/build/${d}/${pat}")
                if (_cands)
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

    set_property(GLOBAL APPEND PROPERTY REGISTERED_COMPONENTS "${NAME}")
endfunction()

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

function(rpp_apply_cxx_utils_shim)
    set(_rpp_root      "${CMAKE_SOURCE_DIR}/lib/redis-plus-plus")
    set(_overlay_root  "${CMAKE_BINARY_DIR}/overlay/redis-plus-plus")
    set(_overlay_inc   "${_overlay_root}")
    file(MAKE_DIRECTORY "${_overlay_inc}/sw/redis++")

    set(_cxx_utils "${_overlay_inc}/sw/redis++/cxx_utils.h")
    file(WRITE "${_cxx_utils}" [=[
#pragma once
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

    set(_tls "${_overlay_inc}/sw/redis++/tls.h")
    file(WRITE "${_tls}" [=[
#pragma once
#if __has_include(<sw/redis++/tls/tls.h>)
#  include <sw/redis++/tls/tls.h>
#elif __has_include(<sw/redis++/no_tls/tls.h>)
#  include <sw/redis++/no_tls/tls.h>
#else
#  include <memory>
#  include <cstddef>
namespace sw { namespace redis { namespace tls {
struct TlsOptions {};
struct NullDeleter { void operator()(void*) const noexcept {} };
using TlsContextUPtr = std::unique_ptr<void, NullDeleter>;
}}} // namespace sw::redis::tls
#endif
]=])

    set(_want_includes
        "${_overlay_inc}"
        "${_rpp_root}/src"
        "${_rpp_root}/build/src"
    )
    foreach(_dir IN LISTS _want_includes)
        if (EXISTS "${_dir}")
            include_directories(BEFORE "${_dir}")
        endif()
    endforeach()

    set(RPP_OVERLAY_INCLUDE_DIR "${_overlay_inc}" PARENT_SCOPE)
endfunction()

function(target_use_all_components tgt)
  get_property(_all GLOBAL PROPERTY REGISTERED_COMPONENTS)
  if(NOT _all)
    return()
  endif()
  foreach(_c IN LISTS _all)
    if(TARGET ${_c}::headers)
      target_link_libraries(${tgt} PRIVATE ${_c}::headers)
    endif()
    if(TARGET ${_c}::lib)
      target_link_libraries(${tgt} PRIVATE ${_c}::lib)
    endif()
  endforeach()
endfunction()

register_component(HIREDIS "lib/hiredis"
    LIB_GLOBS "libhiredis*.so*" "libhiredis*.a" "libhiredis*.dylib"
    INC_REL .
)

register_component(REDISPP "lib/redis-plus-plus"
    LIB_GLOBS "libredis++*.so*" "libredis++*.a" "libredis++*.dylib"
    INC_REL src
)

register_component(CJSON "lib/cJSON"
    LIB_GLOBS "libcjson*.so*" "libcjson*.a" "libcjson*.dylib"
    INC_REL .
)

register_component(ASYNC_SIMPLE "lib/async_simple"
    INC_REL .
)

# 若 async_simple 没有自带 CMakeLists，但头文件存在，则仍视为可用（仅头文件模式），并创建导入目标
if (NOT HAVE_ASYNC_SIMPLE AND EXISTS "${CMAKE_SOURCE_DIR}/lib/async_simple/async_simple/Executor.h")
  set(HAVE_ASYNC_SIMPLE TRUE)
  set(_async_simple_inc "${CMAKE_SOURCE_DIR}/lib/async_simple")
  message(STATUS "Component ASYNC_SIMPLE = AVAILABLE (headers only, no CMakeLists.txt) at ${_async_simple_inc}")
  if (NOT TARGET ASYNC_SIMPLE::headers)
    add_library(ASYNC_SIMPLE::headers INTERFACE IMPORTED)
    set_target_properties(ASYNC_SIMPLE::headers PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${_async_simple_inc}"
    )
  endif()
endif()

rpp_apply_cxx_utils_shim()

get_property(_RPATHS GLOBAL PROPERTY COMPONENT_RPATH_DIRS)
if (_RPATHS)
	list(REMOVE_DUPLICATES _RPATHS)
	string(REPLACE ";" ":" THIRD_RPATH_STR "${_RPATHS}")
	message(STATUS "Setting RPATH to: ${THIRD_RPATH_STR}")
	set(CMAKE_INSTALL_RPATH "${THIRD_RPATH_STR}")
	set(CMAKE_BUILD_RPATH   "${THIRD_RPATH_STR}")
endif()

install(DIRECTORY include/mental1104 DESTINATION include)

# ====================== 新增：自动聚合 src -> 共享库 ======================
file(GLOB_RECURSE M1104_CORE_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.c"
)
if (M1104_CORE_SOURCES)
  add_library(mental1104 SHARED ${M1104_CORE_SOURCES})
else()
  add_library(mental1104 SHARED EXCLUDE_FROM_ALL)
endif()
target_include_directories(mental1104 PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)
target_compile_features(mental1104 PUBLIC cxx_std_20)
set_target_properties(mental1104 PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME "mental1104"
)
if (HAVE_ASYNC_SIMPLE AND TARGET ASYNC_SIMPLE::headers)
  target_link_libraries(mental1104 PUBLIC ASYNC_SIMPLE::headers)
  target_compile_definitions(mental1104 PUBLIC M1104_HAS_ASYNC_SIMPLE=1)
endif()
install(TARGETS mental1104
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
# ========================================================================

enable_testing()

set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS "${TEST_DIR}/*.cpp")
list(LENGTH TEST_FILES _TEST_COUNT)
message(STATUS "Collected ${_TEST_COUNT} test files under ${TEST_DIR} (recursive)")

set(TEST_DEPS_test_redis_lock "HIREDIS;REDISPP")
set(TEST_REQUIRE_LIBS_test_redis_lock "HIREDIS;REDISPP")
set(TEST_DEPS_test_json         "CJSON")
set(TEST_REQUIRE_LIBS_test_json "CJSON")

function(add_optional_test SRC)
	get_filename_component(TEST_NAME "${SRC}" NAME_WE)
	message(STATUS "Consider test ${TEST_NAME} -> ${SRC}")

	set(_hdr_var  "TEST_DEPS_${TEST_NAME}")
	set(_libs_var "TEST_REQUIRE_LIBS_${TEST_NAME}")
	set(req_headers "${${_hdr_var}}")
	set(req_libs    "${${_libs_var}}")

	set(missing "")
	foreach(dep IN LISTS req_headers req_libs)
		if (dep AND NOT HAVE_${dep})
			list(APPEND missing "${dep}")
		endif()
	endforeach()
	foreach(dep IN LISTS req_libs)
		if (dep AND NOT TARGET ${dep}::lib)
			list(APPEND missing "${dep}(lib)")
		endif()
	endforeach()
	if (missing)
		message(STATUS "Skip test ${TEST_NAME}: missing => ${missing}")
		return()
	endif()

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

	# 关键：所有测试默认链接到 libmental1104
	if (TARGET mental1104)
		target_link_libraries(${TEST_NAME} PRIVATE mental1104)
	endif()

	# 如果 async_simple 可用，显式为测试目标添加头路径与宏，避免 __has_include 失败
	if (HAVE_ASYNC_SIMPLE AND TARGET ASYNC_SIMPLE::headers)
		target_link_libraries(${TEST_NAME} PRIVATE ASYNC_SIMPLE::headers)
		target_compile_definitions(${TEST_NAME} PRIVATE M1104_HAS_ASYNC_SIMPLE=1)
	endif()

	target_link_libraries(${TEST_NAME} PRIVATE
		${GTEST_LINK}
		mpfr gmp
		pthread
	)

	target_use_all_components(${TEST_NAME})

	add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
	message(STATUS "Added test ${TEST_NAME}")
endfunction()

foreach(SRC ${TEST_FILES})
	add_optional_test("${SRC}")
endforeach()

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

if (HAVE_GOOGLE_BENCHMARK)
  set(BENCH_DIR ${CMAKE_SOURCE_DIR}/bench)
  if (EXISTS "${BENCH_DIR}")
    file(GLOB BENCH_SRCS CONFIGURE_DEPENDS "${BENCH_DIR}/*.cpp")
	if (BENCH_SRCS)
	  foreach(SRC ${BENCH_SRCS})
	    get_filename_component(BNAME "${SRC}" NAME_WE)
	    set(TGT "bench_${BNAME}")
	    add_executable(${TGT} "${SRC}")
        target_include_directories(${TGT} PRIVATE
          ${PROJECT_SOURCE_DIR}/include
        )
        target_compile_features(${TGT} PRIVATE cxx_std_20)
        # 关键：bench 也默认链接 libmental1104
        target_link_libraries(${TGT} PRIVATE mental1104)
        target_link_libraries(${TGT} PRIVATE benchmark::benchmark)
        target_use_all_components(${TGT})
        set_target_properties(${TGT} PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        add_test(
          NAME ${TGT}
          COMMAND ${CMAKE_BINARY_DIR}/bin/${TGT}
                  --benchmark_display_aggregates_only=true
                  --benchmark_time_unit=ms
        )
        set_tests_properties(${TGT} PROPERTIES LABELS "bench")
      endforeach()
    else()
      message(STATUS "No benchmark sources under ${BENCH_DIR}")
    endif()
  else()
    message(STATUS "Benchmark dir not found: ${BENCH_DIR}")
  endif()
endif()
