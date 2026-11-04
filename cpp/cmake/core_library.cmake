install(DIRECTORY include/mental1104 DESTINATION include)

file(GLOB_RECURSE M1104_CORE_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.c"
)
# Web 模块独立链接，不能经递归 glob 混入原有主库或抬高其语言标准。
list(FILTER M1104_CORE_SOURCES EXCLUDE REGEX "/src/web/")

# 实现迁入共享库后仍按能力检查依赖，避免原来的可选头文件变成基础库的硬依赖。
option(M1104_ENABLE_JSON "Build the JSON runtime when dependencies are available" ON)
option(M1104_ENABLE_REDIS_LOCK "Build Redis lock runtime when client libraries are available" ON)
option(M1104_ENABLE_HIGH_PRECISION "Build MPFR calculators when dependencies are available" ON)
option(M1104_ENABLE_BOOST_ASIO "Build the Boost.Asio executor when headers are available" ON)
find_path(M1104_BOOST_INCLUDE_DIR boost/asio/thread_pool.hpp
  PATHS "${PROJECT_SOURCE_DIR}/lib/boost" "${PROJECT_SOURCE_DIR}/lib/boost/include"
    "${PROJECT_SOURCE_DIR}/lib/boost/libs/asio/include")
find_library(M1104_MPFR_LIBRARY NAMES mpfr)
find_library(M1104_GMP_LIBRARY NAMES gmp)
find_path(M1104_MPFR_INCLUDE_DIR mpfr.h)
find_path(M1104_MULTIPRECISION_INCLUDE_DIR boost/multiprecision/mpfr.hpp
  PATHS "${PROJECT_SOURCE_DIR}/lib/boost" "${PROJECT_SOURCE_DIR}/lib/boost/include"
    "${PROJECT_SOURCE_DIR}/lib/boost/libs/multiprecision/include")

# 某些 Boost.Math 版本要求 C++14；按当前标准试编译，不能让可选算法抬高基础库标准。
set(_m1104_numeric_compatible FALSE)
if(M1104_ENABLE_HIGH_PRECISION AND NOT WIN32 AND M1104_MPFR_LIBRARY AND M1104_GMP_LIBRARY
    AND M1104_MPFR_INCLUDE_DIR AND M1104_MULTIPRECISION_INCLUDE_DIR)
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${THIRD_INCLUDE_DIRS} "${M1104_MULTIPRECISION_INCLUDE_DIR}" "${M1104_MPFR_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES "${M1104_MPFR_LIBRARY}" "${M1104_GMP_LIBRARY}")
  check_cxx_source_compiles("#include <boost/multiprecision/mpfr.hpp>
int main() { boost::multiprecision::mpfr_float value(1); return value == 1 ? 0 : 1; }"
    M1104_MPFR_COMPATIBLE_CXX${CMAKE_CXX_STANDARD})
  set(_m1104_numeric_compatible "${M1104_MPFR_COMPATIBLE_CXX${CMAKE_CXX_STANDARD}}")
  cmake_pop_check_state()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/json.cmake")
set(M1104_HAS_REDIS_LOCK FALSE)
if(M1104_ENABLE_REDIS_LOCK AND TARGET HIREDIS::lib AND TARGET REDISPP::lib)
  set(M1104_HAS_REDIS_LOCK TRUE)
else()
  list(FILTER M1104_CORE_SOURCES EXCLUDE REGEX "/src/redis_lock\\.cpp$")
endif()
set(M1104_HAS_HIGH_PRECISION FALSE)
if(_m1104_numeric_compatible)
  set(M1104_HAS_HIGH_PRECISION TRUE)
else()
  list(FILTER M1104_CORE_SOURCES EXCLUDE REGEX "/src/high_precision_decimal\\.cpp$")
endif()
set(M1104_HAS_BOOST_ASIO FALSE)
if(M1104_ENABLE_BOOST_ASIO AND M1104_BOOST_INCLUDE_DIR)
  set(M1104_HAS_BOOST_ASIO TRUE)
else()
  list(FILTER M1104_CORE_SOURCES EXCLUDE REGEX "/src/concurrency/boost_asio_executor\\.cpp$")
endif()
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
find_package(Threads REQUIRED)
target_link_libraries(mental1104 PUBLIC Threads::Threads)
if(M1104_HAS_JSON_RAPIDJSON)
  target_include_directories(mental1104 SYSTEM PRIVATE "${M1104_RAPIDJSON_INCLUDE_DIR}")
endif()
if(M1104_HAS_JSON_CJSON)
  target_link_libraries(mental1104 PRIVATE CJSON::headers CJSON::lib)
endif()
if(M1104_HAS_REDIS_LOCK)
  # 公开头与实现必须采用相同 redis++ 标准配置，链接库则属于运行时实现依赖。
  target_link_libraries(mental1104 PUBLIC REDISPP::headers HIREDIS::headers
    PRIVATE REDISPP::lib HIREDIS::lib)
endif()
if(M1104_HAS_HIGH_PRECISION)
  target_include_directories(mental1104 PUBLIC "${M1104_MPFR_INCLUDE_DIR}" "${M1104_MULTIPRECISION_INCLUDE_DIR}")
  # 公共 high_precision 返回类型的析构也可能在消费端调用 MPFR/GMP。
  target_link_libraries(mental1104 PUBLIC "${M1104_MPFR_LIBRARY}" "${M1104_GMP_LIBRARY}")
endif()
if(M1104_BOOST_INCLUDE_DIR)
  target_include_directories(mental1104 PUBLIC "${M1104_BOOST_INCLUDE_DIR}")
endif()
if(M1104_HAS_BOOST_ASIO OR M1104_HAS_HIGH_PRECISION OR M1104_HAS_JSON)
  # 稀疏 Boost 由多个真实 include 根组成，不能安装根目录中不存在的聚合头树。
  # 同时向 target 消费端传播这些根，避免静默改用开发机的另一版 Boost。
  file(GLOB _m1104_boost_header_roots "${PROJECT_SOURCE_DIR}/lib/boost/libs/*/include/boost")
  foreach(_header_root IN LISTS _m1104_boost_header_roots)
    get_filename_component(_include_root "${_header_root}" DIRECTORY)
    target_include_directories(mental1104 PUBLIC "${_include_root}")
    install(DIRECTORY "${_header_root}/" DESTINATION include/boost)
  endforeach()
endif()
foreach(_capability JSON JSON_CJSON JSON_RAPIDJSON REDIS_LOCK HIGH_PRECISION BOOST_ASIO)
  target_compile_definitions(mental1104 PUBLIC
    M1104_HAS_${_capability}=$<BOOL:${M1104_HAS_${_capability}}>)
  message(STATUS "Compiled ${_capability} runtime: ${M1104_HAS_${_capability}}")
endforeach()
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
  # 安装消费端需要完整的公共基类头，不能只继承构建目录中的 include 路径。
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/lib/async_simple/async_simple"
    DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endif()
set(M1104_BUILD_HAS_ASYNC_SIMPLE ${HAVE_ASYNC_SIMPLE})
configure_file("${PROJECT_SOURCE_DIR}/cmake/build_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/mental1104/build_config.h" @ONLY)
target_include_directories(mental1104 BEFORE PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/mental1104/build_config.h"
  DESTINATION include/mental1104)
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
