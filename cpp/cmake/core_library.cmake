install(DIRECTORY include/mental1104 DESTINATION include)

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
