# 供本机自动准备和显式 M1104_INSTALL_MQ_DEPS 共用；包含文件本身不安装包。

# 检查 Debian 包是否已安装，不修改系统。
# @param result 接收布尔结果的父作用域变量名。
# @param ARGN 要求全部已安装的包名。
function(_m1104_mq_packages_installed result)
  foreach(_package IN LISTS ARGN)
    execute_process(COMMAND dpkg-query -W "-f=\${Status}" "${_package}"
      RESULT_VARIABLE _status OUTPUT_VARIABLE _state ERROR_QUIET)
    if(NOT _status EQUAL 0 OR NOT _state STREQUAL "install ok installed")
      set(${result} FALSE PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${result} TRUE PARENT_SCOPE)
endfunction()

# 执行准备流程选择的 apt 命令，失败时停止配置，不静默留下禁用的 SDK。
# @param ARGN 完整命令及参数；参数按列表传递，不经过 shell 拼接。
function(_m1104_mq_run_apt)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE _status)
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Dependency installation failed (${_status}); install the packages with apt or provide SDK paths explicitly")
  endif()
endfunction()

# 下载 Apache 官方的一个 Pulsar DEB，校验同一发布目录中的 SHA512。
# @param filename 官方 DEB 文件名。
# @param result 接收已校验本地绝对路径的父作用域变量名。
# 版本和架构由安装流程确定；失败时不把文件交给 apt。
function(_m1104_mq_download_pulsar_deb filename result)
  set(_base_url "https://archive.apache.org/dist/pulsar/pulsar-client-cpp-4.2.0/deb-${_m1104_pulsar_arch}")
  set(_directory "${CMAKE_BINARY_DIR}/mq-packages/pulsar-4.2.0/${_m1104_pulsar_arch}")
  file(MAKE_DIRECTORY "${_directory}")
  file(DOWNLOAD "${_base_url}/${filename}.sha512" "${_directory}/${filename}.sha512"
    TLS_VERIFY ON TIMEOUT 60 STATUS _download_status)
  list(GET _download_status 0 _status)
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Cannot download Pulsar checksum: ${_download_status}")
  endif()
  file(READ "${_directory}/${filename}.sha512" _checksum_file)
  string(REGEX MATCH "^[0-9a-fA-F]+" _checksum "${_checksum_file}")
  string(LENGTH "${_checksum}" _checksum_length)
  if(NOT _checksum_length EQUAL 128)
    message(FATAL_ERROR "Invalid Pulsar SHA512 checksum for ${filename}")
  endif()
  file(DOWNLOAD "${_base_url}/${filename}" "${_directory}/${filename}"
    EXPECTED_HASH "SHA512=${_checksum}" TLS_VERIFY ON TIMEOUT 300
    STATUS _download_status)
  list(GET _download_status 0 _status)
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Cannot download Pulsar package: ${_download_status}")
  endif()
  set(${result} "${_directory}/${filename}" PARENT_SCOPE)
endfunction()

# 为已启用且当前标准支持的后端安装 SDK；已安装的包直接复用。
# Kafka 优先使用发行版仓库；Pulsar 优先使用已配置仓库，随后使用官方 4.2.0 DEB。
# 会修改构建宿主机系统包；非 root 仅尝试 sudo -n，绝不等待密码。
function(m1104_install_mq_apt_dependencies)
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "MQ apt preparation is for native builds; supply target SDK paths when cross compiling")
  endif()
  if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" OR NOT EXISTS "/etc/debian_version")
    message(FATAL_ERROR "M1104_INSTALL_MQ_DEPS requires Debian/Ubuntu apt; install native SDKs or set their CMake paths on this platform")
  endif()
  find_program(_m1104_apt_get apt-get)
  find_program(_m1104_apt_cache apt-cache)
  if(NOT _m1104_apt_get OR NOT _m1104_apt_cache)
    message(FATAL_ERROR "apt-get and apt-cache are required for MQ dependency preparation")
  endif()

  set(_need_kafka FALSE)
  set(_need_pulsar FALSE)
  if(M1104_ENABLE_KAFKA)
    _m1104_mq_packages_installed(_installed librdkafka-dev)
    if(NOT _installed)
      set(_need_kafka TRUE)
    endif()
  endif()
  if(M1104_ENABLE_PULSAR AND NOT CMAKE_CXX_STANDARD LESS 17)
    _m1104_mq_packages_installed(_installed apache-pulsar-client apache-pulsar-client-dev)
    if(NOT _installed)
      set(_need_pulsar TRUE)
    endif()
  endif()
  if(NOT _need_kafka AND NOT _need_pulsar)
    message(STATUS "MQ apt packages are already installed or not requested")
    return()
  endif()

  execute_process(COMMAND id -u OUTPUT_VARIABLE _uid OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _status)
  set(_apt_prefix "")
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Cannot determine privileges for explicit MQ apt installation")
  elseif(NOT _uid STREQUAL "0")
    find_program(_m1104_sudo sudo)
    if(NOT _m1104_sudo)
      message(FATAL_ERROR "Run MQ dependency preparation as root or install sudo with noninteractive apt permission")
    endif()
    set(_apt_prefix "${_m1104_sudo}" -n)
  endif()
  _m1104_mq_run_apt(${_apt_prefix} "${_m1104_apt_get}" update)

  set(_packages "")
  if(_need_kafka)
    list(APPEND _packages librdkafka-dev)
  endif()
  if(_need_pulsar)
    execute_process(COMMAND "${_m1104_apt_cache}" show apache-pulsar-client-dev
      RESULT_VARIABLE _native_package OUTPUT_QUIET ERROR_QUIET)
    if(_native_package EQUAL 0)
      list(APPEND _packages apache-pulsar-client apache-pulsar-client-dev)
    else()
      execute_process(COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE _architecture OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _status)
      if(_status EQUAL 0 AND _architecture STREQUAL "amd64")
        set(_m1104_pulsar_arch x86_64)
      elseif(_status EQUAL 0 AND _architecture STREQUAL "arm64")
        set(_m1104_pulsar_arch arm64)
      else()
        message(FATAL_ERROR "Official Pulsar DEB preparation supports amd64/arm64; provide a native SDK for ${_architecture}")
      endif()
      _m1104_mq_download_pulsar_deb(apache-pulsar-client.deb _runtime)
      _m1104_mq_download_pulsar_deb(apache-pulsar-client-dev.deb _development)
      list(APPEND _packages "${_runtime}" "${_development}")
    endif()
  endif()
  _m1104_mq_run_apt(${_apt_prefix} "${_m1104_apt_get}" install -y --no-install-recommends ${_packages})
endfunction()
