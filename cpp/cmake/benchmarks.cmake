if (HAVE_GOOGLE_BENCHMARK)
  set(BENCH_DIR ${CMAKE_SOURCE_DIR}/bench)
  if (EXISTS "${BENCH_DIR}")
    file(GLOB BENCH_SRCS CONFIGURE_DEPENDS "${BENCH_DIR}/*.cpp")
    if (BENCH_SRCS)
      foreach(SRC ${BENCH_SRCS})
        get_filename_component(BNAME "${SRC}" NAME_WE)
        set(TGT "bench_${BNAME}")
        add_executable(${TGT} "${SRC}")
        target_include_directories(${TGT} PRIVATE
          ${PROJECT_SOURCE_DIR}/include
        )
        target_compile_features(${TGT} PRIVATE cxx_std_20)
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
    else()
      message(STATUS "No benchmark sources under ${BENCH_DIR}")
    endif()
  else()
    message(STATUS "Benchmark dir not found: ${BENCH_DIR}")
  endif()
endif()
