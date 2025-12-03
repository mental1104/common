cmake_minimum_required(VERSION 3.10)
project(mental1104)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(cmake/project_settings.cmake)
include(cmake/deps.cmake)
include(cmake/core_library.cmake)
include(cmake/testing.cmake)
include(cmake/benchmarks.cmake)
include(cmake/coverage.cmake)
