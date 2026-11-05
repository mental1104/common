# Web 目标复用主工程入口，声明、实现与测试保留在各自已有目录。
if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 17)
  message(FATAL_ERROR "mental1104_web requires C++17 or newer; disable M1104_ENABLE_WEB for older core builds")
endif()
if(NOT DEFINED CMAKE_CXX_STANDARD)
  # 先明确标准，再加载第三方 CMake 包，避免旧版 Filesystem 探测写入空标准。
  set(CMAKE_CXX_STANDARD 17)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)
set(_web_root "${CMAKE_CURRENT_LIST_DIR}/..")
option(M1104_WEB_WITH_DROGON "Build the Drogon web adapter" ON)
option(M1104_WEB_BUILD_TESTS "Build web module contract tests" ON)

add_library(mental1104_web SHARED "${_web_root}/src/web/server.cpp")
add_library(mental1104::web ALIAS mental1104_web)
target_compile_features(mental1104_web PUBLIC cxx_std_17)
target_link_libraries(mental1104_web PRIVATE Threads::Threads)
target_include_directories(mental1104_web PUBLIC
  $<BUILD_INTERFACE:${_web_root}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(mental1104_web PROPERTIES EXPORT_NAME web
  WINDOWS_EXPORT_ALL_SYMBOLS ON INSTALL_RPATH "")
set(_web_install_targets mental1104_web)

if(M1104_WEB_WITH_DROGON)
  find_package(Drogon 1.8 CONFIG REQUIRED)
  add_library(mental1104_web_drogon SHARED "${_web_root}/src/web/drogon_server.cpp")
  add_library(mental1104::web_drogon ALIAS mental1104_web_drogon)
  target_link_libraries(mental1104_web_drogon
    PUBLIC mental1104_web PRIVATE Drogon::Drogon Threads::Threads)
  set_target_properties(mental1104_web_drogon PROPERTIES EXPORT_NAME web_drogon
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
  # 安装库同目录可找到基础 Web 库，Drogon 的搜索路径由其安装方式负责。
  if(APPLE)
    set_property(TARGET mental1104_web_drogon PROPERTY INSTALL_RPATH "@loader_path")
  elseif(UNIX)
    set_property(TARGET mental1104_web_drogon PROPERTY INSTALL_RPATH "$ORIGIN")
  endif()
  list(APPEND _web_install_targets mental1104_web_drogon)
endif()

install(TARGETS ${_web_install_targets} EXPORT mental1104WebTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Web
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Web
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Web)
install(DIRECTORY "${_web_root}/include/mental1104/web"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mental1104" COMPONENT Web)
install(EXPORT mental1104WebTargets NAMESPACE mental1104::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mental1104Web" COMPONENT Web)
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/mental1104WebConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/mental1104WebConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mental1104Web")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mental1104WebConfig.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mental1104Web" COMPONENT Web)

if(M1104_WEB_BUILD_TESTS)
  enable_testing()
  if(TARGET gtest_main)
    set(_web_gtest gtest_main)
  else()
    find_package(GTest REQUIRED)
    set(_web_gtest GTest::gtest_main)
  endif()
  add_executable(test_web_lifecycle "${_web_root}/test/unit/web/lifecycle_scenarios.cc")
  target_link_libraries(test_web_lifecycle PRIVATE mental1104_web ${_web_gtest})
  add_test(NAME test_web_lifecycle COMMAND test_web_lifecycle)
  set_tests_properties(test_web_lifecycle PROPERTIES TIMEOUT 20 LABELS "unit")

  # 给任意 Server 派生实现复用相同场景；只有 adapter_source 了解被测 SDK。
  # target: 新测试程序名；backend_target: 后端库目标；adapter_source: 接入文件绝对路径。
  function(m1104_add_web_backend_scenarios target backend_target adapter_source)
    if(NOT TARGET mental1104_web_scenarios)
      find_package(Boost 1.70 REQUIRED)
      find_package(OpenSSL REQUIRED)
      find_program(M1104_WEB_OPENSSL openssl REQUIRED)
      add_library(mental1104_web_wire_client STATIC
        "${_web_root}/test/unit/web/wire_client.cc")
      # 隔离父目录的第三方头搜索路径，测试客户端统一使用 find_package 选出的 Boost。
      set_property(TARGET mental1104_web_wire_client PROPERTY INCLUDE_DIRECTORIES "")
      target_link_libraries(mental1104_web_wire_client PUBLIC mental1104_web
        PRIVATE Boost::boost OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
      add_library(mental1104_web_scenarios OBJECT
        "${_web_root}/test/unit/web/server_scenarios.cc")
      target_link_libraries(mental1104_web_scenarios PUBLIC
        mental1104_web_wire_client ${_web_gtest})
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-tls")
      execute_process(COMMAND "${M1104_WEB_OPENSSL}" req -x509 -newkey rsa:2048
        -nodes -keyout "${CMAKE_CURRENT_BINARY_DIR}/test-tls/key.pem"
        -out "${CMAKE_CURRENT_BINARY_DIR}/test-tls/cert.pem" -days 2 -subj /CN=localhost
        RESULT_VARIABLE _web_certificate_result OUTPUT_QUIET ERROR_QUIET)
      if(NOT _web_certificate_result EQUAL 0)
        message(FATAL_ERROR "Cannot generate the web TLS test certificate")
      endif()
      set_property(TARGET mental1104_web_scenarios PROPERTY M1104_TEST_TLS_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/test-tls")
    endif()
    add_executable(${target} "${adapter_source}")
    target_link_libraries(${target} PRIVATE
      mental1104_web_scenarios ${backend_target})
    target_include_directories(${target} PRIVATE "${_web_root}/test/unit/web")
    include(GoogleTest)
    get_target_property(_tls_dir mental1104_web_scenarios M1104_TEST_TLS_DIR)
    # 每个测试单独启动进程，后端不需要支持重启，场景可并行、筛选和单独重跑。
    gtest_discover_tests(${target} TEST_PREFIX "web.${target}."
      DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 20 LABELS "unit"
      ENVIRONMENT "M1104_TEST_TLS_DIR=${_tls_dir}")
  endfunction()

  if(M1104_WEB_WITH_DROGON)
    m1104_add_web_backend_scenarios(test_web_drogon mental1104_web_drogon
      "${_web_root}/test/unit/web/drogon_backend.cc")
    target_link_libraries(test_web_drogon PRIVATE Drogon::Drogon)
  endif()
endif()
