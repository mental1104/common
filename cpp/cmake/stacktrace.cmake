set(STACKTRACE_SRC
  ${PROJECT_SOURCE_DIR}/src/debug/stacktrace_common.c
)
if (WIN32)
  list(APPEND STACKTRACE_SRC ${PROJECT_SOURCE_DIR}/src/debug/stacktrace_windows.c)
else()
  list(APPEND STACKTRACE_SRC ${PROJECT_SOURCE_DIR}/src/debug/stacktrace_posix.c)
endif()

add_library(stacktrace STATIC ${STACKTRACE_SRC})
target_include_directories(stacktrace PUBLIC ${PROJECT_SOURCE_DIR}/include)

if (WIN32)
  target_link_libraries(stacktrace PUBLIC Dbghelp)
elseif (UNIX AND NOT APPLE)
  target_link_libraries(stacktrace PUBLIC dl)
endif()

option(STACKTRACE_BUILD_EXAMPLES "Build stacktrace examples" ON)
if (STACKTRACE_BUILD_EXAMPLES)
  add_executable(crash_c ${PROJECT_SOURCE_DIR}/examples/debug/stacktrace/crash_c.c)
  target_include_directories(crash_c PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(crash_c PRIVATE stacktrace)
  set_target_properties(crash_c PROPERTIES C_STANDARD 99)

  add_executable(crash_cpp ${PROJECT_SOURCE_DIR}/examples/debug/stacktrace/crash_cpp.cpp)
  target_include_directories(crash_cpp PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(crash_cpp PRIVATE stacktrace)
  set_target_properties(crash_cpp PROPERTIES CXX_STANDARD 11)
endif()
