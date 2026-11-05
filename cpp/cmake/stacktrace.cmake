set(STACKTRACE_SRC
  ${PROJECT_SOURCE_DIR}/src/debug/stacktrace_common.c
)
if (WIN32)
  list(APPEND STACKTRACE_SRC ${PROJECT_SOURCE_DIR}/src/debug/stacktrace_windows.c)
else()
  list(APPEND STACKTRACE_SRC ${PROJECT_SOURCE_DIR}/src/debug/stacktrace_posix.c)
endif()

# 同一平台实现只编译一次，位置无关对象同时用于主共享库与原有静态库产物。
add_library(m1104_stacktrace_objects OBJECT ${STACKTRACE_SRC})
target_include_directories(m1104_stacktrace_objects PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(m1104_stacktrace_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(mental1104 PRIVATE $<TARGET_OBJECTS:m1104_stacktrace_objects>)
add_library(stacktrace STATIC $<TARGET_OBJECTS:m1104_stacktrace_objects>)
target_include_directories(stacktrace PUBLIC ${PROJECT_SOURCE_DIR}/include)

if (WIN32)
  target_link_libraries(stacktrace PUBLIC Dbghelp)
elseif (UNIX AND NOT APPLE)
  target_link_libraries(stacktrace PUBLIC dl)
endif()
