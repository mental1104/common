install(DIRECTORY include/mental1104 DESTINATION include)

file(GLOB_RECURSE M1104_CORE_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.c"
)
if (WIN32)
  list(FILTER M1104_CORE_SOURCES EXCLUDE REGEX "stacktrace_posix\\.c$")
else()
  list(FILTER M1104_CORE_SOURCES EXCLUDE REGEX "stacktrace_windows\\.c$")
endif()
if (M1104_CORE_SOURCES)
  add_library(mental1104 SHARED ${M1104_CORE_SOURCES})
else()
  add_library(mental1104 SHARED EXCLUDE_FROM_ALL)
endif()
target_include_directories(mental1104 PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(mental1104 PROPERTIES
  CXX_STANDARD ${CMAKE_CXX_STANDARD}
  CXX_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME "mental1104"
)
if (WIN32)
  # Auto-export all symbols so an import library is produced without annotating each API.
  set_target_properties(mental1104 PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
if (HAVE_ASYNC_SIMPLE AND TARGET ASYNC_SIMPLE::headers)
  target_link_libraries(mental1104 PUBLIC ASYNC_SIMPLE::headers)
  target_compile_definitions(mental1104 PUBLIC M1104_HAS_ASYNC_SIMPLE=1)
endif()
if (WIN32)
  target_link_libraries(mental1104 PRIVATE Dbghelp)
elseif (UNIX AND NOT APPLE)
  target_link_libraries(mental1104 PRIVATE dl)
endif()
install(TARGETS mental1104
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
