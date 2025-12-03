set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "CXX_STANDARD = ${CMAKE_CXX_STANDARD}")

set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -g")
set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} -Os -DNDEBUG")

include_directories(${PROJECT_SOURCE_DIR}/include)

option(AUTO_FETCH_SUBMODULES "Auto init/update git submodules on configure" ON)
if (AUTO_FETCH_SUBMODULES)
  find_package(Git QUIET)
  if (GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.gitmodules")
    message(STATUS "Auto fetching submodules (git submodule update --init --recursive --depth=1)")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --depth=1
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      RESULT_VARIABLE _subm_res
      OUTPUT_QUIET
      ERROR_QUIET
    )
    if (NOT _subm_res EQUAL 0)
      message(WARNING "git submodule update failed; continue with existing content.")
    endif()
  endif()
endif()
