enable_testing()

set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS "${TEST_DIR}/*.cpp")
list(LENGTH TEST_FILES _TEST_COUNT)
message(STATUS "Collected ${_TEST_COUNT} test files under ${TEST_DIR} (recursive)")

set(M1104_SKIP_TESTS "")
if (WIN32)
  # Windows runners lack mpfr/gmp/pthread; skip unsupported tests
  list(APPEND M1104_SKIP_TESTS
    test_high_precision_decimal
    test_boost_mn_coroutine_pool
    test_mn_coroutine_pool
    test_mn_coroutine_pool_async_simple
  )
  set(M1104_PLATFORM_LIBS "")
else()
  set(M1104_PLATFORM_LIBS mpfr gmp pthread)
endif()
if (DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 20)
  list(APPEND M1104_SKIP_TESTS
    test_async_simple_scheduler
    test_boost_mn_coroutine_pool
    test_mn_coroutine_pool
    test_mn_coroutine_pool_async_simple
  )
endif()

set(TEST_DEPS_test_redis_lock "HIREDIS;REDISPP")
set(TEST_REQUIRE_LIBS_test_redis_lock "HIREDIS;REDISPP")
set(TEST_DEPS_test_json         "CJSON")
set(TEST_REQUIRE_LIBS_test_json "CJSON")

function(add_optional_test SRC)
  get_filename_component(TEST_NAME "${SRC}" NAME_WE)
  message(STATUS "Consider test ${TEST_NAME} -> ${SRC}")

  list(FIND M1104_SKIP_TESTS "${TEST_NAME}" _skip_idx)
  if (NOT _skip_idx EQUAL -1)
    message(STATUS "Skip test ${TEST_NAME}: in skip list")
    return()
  endif()

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

  set(_extra_sources "")
  if (TEST_NAME STREQUAL "test_c_api_compat")
    list(APPEND _extra_sources "${TEST_DIR}/c_api_compat_c_smoke.c")
  endif()

  add_executable(${TEST_NAME} "${SRC}" ${_extra_sources})
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

  if (TARGET mental1104)
    target_link_libraries(${TEST_NAME} PRIVATE mental1104)
  endif()

  if (HAVE_ASYNC_SIMPLE AND TARGET ASYNC_SIMPLE::headers)
    target_link_libraries(${TEST_NAME} PRIVATE ASYNC_SIMPLE::headers)
    target_compile_definitions(${TEST_NAME} PRIVATE M1104_HAS_ASYNC_SIMPLE=1)
  endif()

  target_link_libraries(${TEST_NAME} PRIVATE
    ${GTEST_LINK}
    ${M1104_PLATFORM_LIBS}
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
