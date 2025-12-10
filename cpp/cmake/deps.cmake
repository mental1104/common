# 主流程入口：先按顺序完成测试/基准、第三方路径收集、组件注册、收尾
macro(m1104_setup_deps)
  m1104_setup_testing_and_benchmark()
  m1104_collect_thirdparty_paths()
  m1104_register_components()
  m1104_finalize_thirdparty()
endmacro()

# 以下为各步骤的实现细节
include(CMakeParseArguments)             # 引入 cmake_parse_arguments，供 register_component 解析可选参数列表
set_property(GLOBAL PROPERTY COMPONENT_RPATH_DIRS "") # 预置全局属性，累积第三方库的 rpath 目录
set_property(GLOBAL PROPERTY REGISTERED_COMPONENTS "") # 预置全局属性，记录注册过的组件名
set_property(GLOBAL PROPERTY SUBMODULE_INSTALL_BUILD_DIRS "") # 记录需要在 install 阶段递归安装的子模块 build 目录
option(M1104_INSTALL_SUBMODULES "Install C++ submodules that live under lib/* when running make install" ON)

function(_m1104_track_submodule_install_root root)
  if(NOT root)
    return()
  endif()
  get_property(_roots GLOBAL PROPERTY SUBMODULE_INSTALL_BUILD_DIRS)
  if(NOT _roots)
    set(_roots "")
  endif()
  list(APPEND _roots "${root}")
  list(REMOVE_DUPLICATES _roots)
  set_property(GLOBAL PROPERTY SUBMODULE_INSTALL_BUILD_DIRS "${_roots}")
endfunction()

function(m1104_collect_installable_submodules)
  if (NOT M1104_INSTALL_SUBMODULES)
    return()
  endif()
  file(GLOB _lib_subs RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/lib/*")
  foreach(_rel IN LISTS _lib_subs)
    set(_root "${CMAKE_SOURCE_DIR}/${_rel}")
    if (NOT IS_DIRECTORY "${_root}")
      continue()
    endif()
    if (NOT EXISTS "${_root}/CMakeLists.txt")
      continue()
    endif()
    _m1104_track_submodule_install_root("${_root}/build")
  endforeach()
endfunction()

# 测试/基准相关
macro(m1104_setup_testing_and_benchmark)
  add_subdirectory(thirdparty/googletest) # gtest/gmock 作为子目录源码构建（仓库已自带），无需全局安装/FindGTest

  set(BENCH_THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/thirdparty/benchmark") # 先记录本地 benchmark 源码目录，后面存在才 add_subdirectory 并配置开关
  if (EXISTS "${BENCH_THIRDPARTY_DIR}/CMakeLists.txt")
    # 通过 CACHE 变量强制关闭 gbench 自带测试/安装，避免污染主项目选项。
    # 形式为 set(<var> <val> CACHE BOOL "" FORCE)：
    #   - CACHE BOOL：定义为缓存布尔选项
    #   - ""：描述为空
    #   - FORCE：即便已有缓存值也覆盖为 OFF
    set(BENCHMARK_ENABLE_TESTING      OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS  OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL      OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/benchmark EXCLUDE_FROM_ALL)
    set(HAVE_GOOGLE_BENCHMARK TRUE)
  else()
    set(HAVE_GOOGLE_BENCHMARK FALSE)
  endif()

  include_directories(${gtest_SOURCE_DIR}/include ${gmock_SOURCE_DIR}/include) # 暴露 gtest/gmock 的公共头（如 gtest/gtest.h, gmock/gmock.h）；变量由 add_subdirectory(thirdparty/googletest) 自动设置
  set(GTEST_LINK gtest gtest_main) # 预设一个链接列表变量，后续测试/基准 target 可直接使用
endmacro()

# 第三方目录收集（lib 下扫描 include/lib 子目录，再追加常见头文件路径）
macro(m1104_collect_thirdparty_paths)
  set(THIRDROOT "${CMAKE_SOURCE_DIR}/lib")
  message(STATUS "THIRDROOT = ${THIRDROOT}")

  set(THIRD_INCLUDE_DIRS "") # CMake 中空字符串可作为空列表起始值，后续 list(APPEND ...) 即转换为列表使用
  set(THIRD_LIB_DIRS "")

  if(NOT EXISTS ${THIRDROOT})
    message(STATUS "THIRDROOT not found; skip thirdparty path collection")
    return()
  endif()

  # 在 lib 下扫描形如 */build/include 及其子目录，收集可能的第三方头文件路径
  # 例如如果存在 lib/boost/build/include 或 lib/redis-plus-plus/build/include/sw，它们都会出现在列表中。
  file(GLOB THIRD_INC_CANDIDATES RELATIVE ${CMAKE_SOURCE_DIR} # GLOB 语法：匹配指定模式的路径；RELATIVE 使结果相对源目录，便于后续拼接
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
endmacro()

function(register_component NAME SUBPATH)
    set(options)
    set(oneValueArgs BUILD_SUBDIR)
    set(multiValueArgs INC_REL LIB_GLOBS)
    cmake_parse_arguments(CMP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(_root "${CMAKE_SOURCE_DIR}/${SUBPATH}")
    if (CMP_BUILD_SUBDIR)
        set(_build_dir "${_root}/${CMP_BUILD_SUBDIR}")
    else()
        set(_build_dir "${_root}/build")
    endif()

    if (EXISTS "${_root}/CMakeLists.txt")
        set(HAVE_${NAME} TRUE PARENT_SCOPE)
        message(STATUS "Component ${NAME} = AVAILABLE at ${SUBPATH}")
        _m1104_track_submodule_install_root("${_build_dir}")
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
    if (EXISTS "${_build_dir}")
        foreach(d "" "lib" "src")
            foreach(pat IN LISTS CMP_LIB_GLOBS)
                file(GLOB _cands "${_build_dir}/${d}/${pat}")
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

# 组件注册与收尾
macro(m1104_register_components)
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

  if (HAVE_ASYNC_SIMPLE)
    set(_async_simple_header "${CMAKE_SOURCE_DIR}/lib/async_simple/async_simple/Executor.h")
    if (NOT EXISTS "${_async_simple_header}")
      message(WARNING "async_simple component declared but header missing: ${_async_simple_header}; disabling ASYNC_SIMPLE")
      set(HAVE_ASYNC_SIMPLE FALSE)
    endif()
  endif()

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
endmacro()

macro(m1104_finalize_thirdparty)
  rpp_apply_cxx_utils_shim()

  get_property(_RPATHS GLOBAL PROPERTY COMPONENT_RPATH_DIRS)
  if (_RPATHS)
    list(REMOVE_DUPLICATES _RPATHS)
    string(REPLACE ";" ":" THIRD_RPATH_STR "${_RPATHS}")
    message(STATUS "Setting RPATH to: ${THIRD_RPATH_STR}")
    set(CMAKE_INSTALL_RPATH "${THIRD_RPATH_STR}")
    set(CMAKE_BUILD_RPATH   "${THIRD_RPATH_STR}")
  endif()
endmacro()

function(m1104_emit_submodule_install_rules)
  if (NOT M1104_INSTALL_SUBMODULES)
    message(STATUS "M1104_INSTALL_SUBMODULES=OFF -> skip submodule installs")
    return()
  endif()

  m1104_collect_installable_submodules()
  get_property(_submods GLOBAL PROPERTY SUBMODULE_INSTALL_BUILD_DIRS)
  if (NOT _submods)
    message(STATUS "No submodule install roots recorded; skip")
    return()
  endif()
  list(REMOVE_DUPLICATES _submods)

  set(_code "set(_m1104_submodule_builds\n")
  foreach(_p IN LISTS _submods)
    file(TO_CMAKE_PATH "${_p}" _norm)
    string(APPEND _code "  \"${_norm}\"\n")
  endforeach()
  string(APPEND _code ")\n")
  string(APPEND _code [=[
foreach(_build_dir IN LISTS _m1104_submodule_builds)
  if(NOT EXISTS "${_build_dir}")
    message(STATUS "[submodule-install] skip ${_build_dir} (build dir missing)")
    continue()
  endif()
  if(NOT EXISTS "${_build_dir}/cmake_install.cmake")
    message(STATUS "[submodule-install] skip ${_build_dir} (cmake_install.cmake missing)")
    continue()
  endif()
  message(STATUS "[submodule-install] ${CMAKE_COMMAND} --install ${_build_dir} --prefix ${CMAKE_INSTALL_PREFIX}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${_build_dir}" --prefix "${CMAKE_INSTALL_PREFIX}"
    RESULT_VARIABLE _m1104_submod_res
  )
  if(NOT _m1104_submod_res EQUAL 0)
    message(FATAL_ERROR "[submodule-install] failed for ${_build_dir} (code=${_m1104_submod_res})")
  endif()
endforeach()
]=])

  install(CODE "${_code}")
endfunction()

# 执行主流程
m1104_setup_deps()
m1104_emit_submodule_install_rules()
