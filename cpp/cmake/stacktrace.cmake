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
