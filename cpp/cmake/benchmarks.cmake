if (HAVE_GOOGLE_BENCHMARK)
  set(BENCH_DIR ${CMAKE_SOURCE_DIR}/bench)
  if (EXISTS "${BENCH_DIR}")
    file(GLOB BENCH_SRCS CONFIGURE_DEPENDS "${BENCH_DIR}/*.cpp")
    if (BENCH_SRCS)
      set(_bench_targets "")
      if (DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 20)
        set(_skip_benches bench_mn_coroutine_pool)
      else()
        set(_skip_benches "")
      endif()
      foreach(SRC ${BENCH_SRCS})
        get_filename_component(BNAME "${SRC}" NAME_WE)
        list(FIND _skip_benches "${BNAME}" _skip_idx)
        if (NOT _skip_idx EQUAL -1)
          message(STATUS "Skip benchmark ${BNAME}: requires C++20")
          continue()
        endif()
        set(TGT "bench_${BNAME}")
        add_executable(${TGT} EXCLUDE_FROM_ALL "${SRC}")
        list(APPEND _bench_targets ${TGT})
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
        add_test(
          NAME ${TGT}
          COMMAND ${CMAKE_BINARY_DIR}/bin/${TGT}
                  --benchmark_display_aggregates_only=true
                  --benchmark_time_unit=ms
        )
        set_tests_properties(${TGT} PROPERTIES LABELS "bench")
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
