cmake_minimum_required(VERSION 3.15)
project(mental1104_export_layer_json LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXPORT_BUILD_PYBIND11 "Build pybind11 Python extension" ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(THIRDROOT "${REPO_ROOT}/cpp/lib")

set(EXPORT_JSON_SRCS
    src/json.cpp
    src/json_capi.cpp
)

# Optional cJSON implementation (header is required by mental1104::json)
set(_cjson_src "${THIRDROOT}/cJSON/cJSON.c")
if(EXISTS "${_cjson_src}")
    list(APPEND EXPORT_JSON_SRCS "${_cjson_src}")
    set_source_files_properties("${_cjson_src}" PROPERTIES LANGUAGE C)
endif()

add_library(export_json SHARED ${EXPORT_JSON_SRCS})

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
