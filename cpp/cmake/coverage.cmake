option(ENABLE_COVERAGE "Use gcov" ON)
message(STATUS "ENABLE_COVERAGE=${ENABLE_COVERAGE}")
if(ENABLE_COVERAGE)
  set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  option(COVERAGE "Enable coverage reporting" ON)
  if(COVERAGE)
    message(STATUS "Building with coverage support")
    set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
  endif()
endif()

if(COVERAGE)
  find_program(LCOV_EXEC lcov REQUIRED)
  find_program(GENHTML_EXEC genhtml REQUIRED)
  if(LCOV_EXEC AND GENHTML_EXEC)
    add_custom_target(coverage
      COMMAND ${LCOV_EXEC}
              --directory ${CMAKE_BINARY_DIR}
              --capture
              --ignore-errors mismatch,inconsistent
              --rc geninfo_unexecuted_blocks=1
              --output-file coverage.info
      COMMAND ${LCOV_EXEC}
              --remove coverage.info
              '*/test/*' '/usr/*' '*/external/*' '*/gtest/*'
              --ignore-errors unused
              --output-file coverage_filtered.info
      COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C
              ${LCOV_EXEC} --list coverage_filtered.info
              --ignore-errors mismatch,inconsistent
              --rc geninfo_unexecuted_blocks=1
      COMMAND ${GENHTML_EXEC} coverage_filtered.info
              --output-directory ${CMAKE_BINARY_DIR}/coverage_report
      COMMENT "Generating coverage report..."
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
  else()
    message(WARNING "lcov or genhtml not found, coverage target will not work")
  endif()
endif()
