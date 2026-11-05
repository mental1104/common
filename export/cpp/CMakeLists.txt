cmake_minimum_required(VERSION 3.15)
project(mental1104_export_layer_json LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXPORT_BUILD_PYBIND11 "Build pybind11 Python extension" ON)
option(M1104_ENABLE_JSON_NLOHMANN "Enable the nlohmann JSON parser backend" ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(THIRDROOT "${REPO_ROOT}/cpp/lib")

file(GLOB_RECURSE EXPORT_JSON_CORE_SRCS CONFIGURE_DEPENDS
    "${REPO_ROOT}/cpp/src/json/*.cpp")
set(EXPORT_JSON_SRCS
    src/json.cpp
    src/json_capi.cpp
    # 递归复用公共核心及 detail 后端，避免遗漏私有实现的链接定义。
    ${EXPORT_JSON_CORE_SRCS}
)

# 独立导出工程生成自己的能力配置，不读取开发机安装的另一份库配置。
set(M1104_BUILD_HAS_ASYNC_SIMPLE FALSE)
set(M1104_HAS_JSON TRUE)
set(M1104_HAS_JSON_CJSON FALSE)
set(M1104_HAS_JSON_RAPIDJSON FALSE)
set(M1104_HAS_JSON_NLOHMANN FALSE)
find_path(EXPORT_RAPIDJSON_INCLUDE_DIR rapidjson/document.h
    PATHS "${THIRDROOT}/rapidjson/include")
if(EXPORT_RAPIDJSON_INCLUDE_DIR)
    set(M1104_HAS_JSON_RAPIDJSON TRUE)
endif()
find_path(EXPORT_NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    HINTS "${THIRDROOT}/nlohmann_json/single_include"
        "${THIRDROOT}/nlohmann_json/include")
if(M1104_ENABLE_JSON_NLOHMANN AND EXPORT_NLOHMANN_JSON_INCLUDE_DIR)
    set(M1104_HAS_JSON_NLOHMANN TRUE)
endif()

# cJSON 为导出库的私有原生实现，缺失时保留明确的后端错误。
set(_cjson_src "${THIRDROOT}/cJSON/cJSON.c")
if(EXISTS "${_cjson_src}")
    set(M1104_HAS_JSON_CJSON TRUE)
    list(APPEND EXPORT_JSON_SRCS "${_cjson_src}")
    set_source_files_properties("${_cjson_src}" PROPERTIES LANGUAGE C)
endif()

add_library(export_json SHARED ${EXPORT_JSON_SRCS})
configure_file("${REPO_ROOT}/cpp/cmake/build_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/mental1104/build_config.h" @ONLY)
target_include_directories(export_json BEFORE PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/include")
if(EXPORT_RAPIDJSON_INCLUDE_DIR)
    target_include_directories(export_json PRIVATE "${EXPORT_RAPIDJSON_INCLUDE_DIR}")
endif()
if(M1104_HAS_JSON_NLOHMANN)
    target_include_directories(export_json SYSTEM PRIVATE "${EXPORT_NLOHMANN_JSON_INCLUDE_DIR}")
endif()

set(THIRD_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include"   # our own headers
    "${REPO_ROOT}/cpp/include"              # project public headers
)

# Discover built include dirs under cpp/lib/*/build/include*
file(GLOB _third_inc_candidates
    "${THIRDROOT}/*/build/include"
    "${THIRDROOT}/*/build/include/*"
)
foreach(inc ${_third_inc_candidates})
    if(EXISTS "${inc}")
        list(APPEND THIRD_INCLUDE_DIRS "${inc}")
    endif()
endforeach()

# Fallback direct headers for common libs (header-only rapidjson, cJSON headers, etc.)
foreach(_d
    "${THIRDROOT}/rapidjson/include"
    "${THIRDROOT}/cJSON"
    "${THIRDROOT}/hiredis"
    "${THIRDROOT}/pystring"
    "${THIRDROOT}/spdlog/include"
)
    if(EXISTS "${_d}")
        list(APPEND THIRD_INCLUDE_DIRS "${_d}")
    endif()
endforeach()

# Ensure cJSON headers are reachable even if include paths differ across platforms
set(_cjson_hdr "${THIRDROOT}/cJSON/cJSON.h")
set(_cjson_inc_candidates
    "${THIRDROOT}/cJSON"
    "${THIRDROOT}/cJSON/build"
    "${THIRDROOT}/cJSON/build/include"
)
foreach(_inc IN LISTS _cjson_inc_candidates)
    if(EXISTS "${_inc}")
        list(APPEND THIRD_INCLUDE_DIRS "${_inc}")
    endif()
endforeach()
if(EXISTS "${_cjson_hdr}")
    set(_cjson_overlay "${CMAKE_CURRENT_BINARY_DIR}/cjson_overlay")
    file(MAKE_DIRECTORY "${_cjson_overlay}")
    file(COPY "${_cjson_hdr}" DESTINATION "${_cjson_overlay}")
    list(APPEND THIRD_INCLUDE_DIRS "${_cjson_overlay}")
    # also attach overlay directly in case list is pruned by generator nuances
    target_include_directories(export_json PUBLIC "${_cjson_overlay}")
endif()

list(REMOVE_DUPLICATES THIRD_INCLUDE_DIRS)
target_include_directories(export_json PUBLIC ${THIRD_INCLUDE_DIRS})

if(EXPORT_BUILD_PYBIND11)
    find_package(pybind11 CONFIG QUIET)
    if(pybind11_FOUND)
        pybind11_add_module(mental1104_export_layer_pybind bindings/pybind_module.cpp)
        target_link_libraries(mental1104_export_layer_pybind PRIVATE export_json)
        target_include_directories(mental1104_export_layer_pybind PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    else()
        message(WARNING "pybind11 not found; skipping mental1104_export_layer_pybind build. Set EXPORT_BUILD_PYBIND11=OFF to silence.")
    endif()
endif()
