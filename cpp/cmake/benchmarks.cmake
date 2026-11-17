option(M1104_REGISTER_BENCHMARK_TESTS "Build benchmarks and include them in ordinary CTest runs" OFF)
set(_m1104_benchmark_catalog "")
if (HAVE_GOOGLE_BENCHMARK)
  set(BENCH_DIR ${PROJECT_SOURCE_DIR}/test/bench)
  if (EXISTS "${BENCH_DIR}")
    file(GLOB_RECURSE BENCH_SRCS CONFIGURE_DEPENDS "${BENCH_DIR}/*.cpp")
    if (BENCH_SRCS)
      set(_bench_targets "")
      if (DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 20)
        set(_skip_benches bench_mn_coroutine_pool)
      else()
        set(_skip_benches "")
      endif()
      if(NOT M1104_HAS_JSON)
        list(APPEND _skip_benches bench_json_gbench)
      endif()
      if(NOT M1104_HAS_BOOST_ASIO)
        list(APPEND _skip_benches bench_mn_coroutine_pool)
      endif()
      foreach(SRC ${BENCH_SRCS})
        get_filename_component(BNAME "${SRC}" NAME_WE)
        list(FIND _skip_benches "${BNAME}" _skip_idx)
        if (NOT _skip_idx EQUAL -1)
          message(STATUS "Skip benchmark ${BNAME}: unsupported standard or disabled runtime dependency")
          continue()
        endif()
        set(TGT "bench_${BNAME}")
        if(M1104_REGISTER_BENCHMARK_TESTS)
          add_executable(${TGT} "${SRC}")
        else()
          add_executable(${TGT} EXCLUDE_FROM_ALL "${SRC}")
        endif()
        list(APPEND _bench_targets ${TGT})
        # 由生成器记录当前配置实际定义的目标，不扫描 bin 中可能遗留的旧程序。
        string(APPEND _m1104_benchmark_catalog "$<TARGET_FILE:${TGT}>\n")
        target_include_directories(${TGT} PRIVATE
          ${PROJECT_SOURCE_DIR}/include
        )
        set_target_properties(${TGT} PROPERTIES
          CXX_STANDARD ${CMAKE_CXX_STANDARD}
          CXX_STANDARD_REQUIRED ON
        )
        target_link_libraries(${TGT} PRIVATE mental1104)
        target_link_libraries(${TGT} PRIVATE benchmark::benchmark)
        target_use_all_components(${TGT})
        set_target_properties(${TGT} PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        if(M1104_REGISTER_BENCHMARK_TESTS)
          add_test(
            NAME ${TGT}
            COMMAND ${CMAKE_BINARY_DIR}/bin/${TGT}
                    --benchmark_display_aggregates_only=true
                    --benchmark_time_unit=ms
          )
          set_tests_properties(${TGT} PROPERTIES LABELS "bench")
        endif()
      endforeach()
      if (_bench_targets)
        add_custom_target(m1104_benchmarks DEPENDS ${_bench_targets})
      endif()
    else()
      message(STATUS "No benchmark sources under ${BENCH_DIR}")
    endif()
  else()
    message(STATUS "Benchmark dir not found: ${BENCH_DIR}")
  endif()
endif()

# 默认不把基准加入 ALL 或 CTest；显式聚合目标始终复用同一份库与基准编译产物。
# 按配置分别生成，兼容单配置生成器与 Visual Studio/Ninja Multi-Config。
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/m1104-benchmarks-$<CONFIG>.txt"
  CONTENT "${_m1104_benchmark_catalog}")
