#include "mental1104/core/cache.h"
#include <benchmark/benchmark.h>
#include <cmath>
#include <cstdint>

using mental1104::make_cache;
using mental1104::make_lru_cache;

static inline uint64_t workload(uint64_t n) {
  double x = 0.0;
  for (uint64_t i = 0; i < n; ++i) {
    x += std::sin(static_cast<double>(i)) * std::cos(static_cast<double>(i));
  }
  return static_cast<uint64_t>(x + 12345.0);
}

static void BM_Direct(benchmark::State &state) {
  const uint64_t n = static_cast<uint64_t>(state.range(0));
  for (auto _ : state) {
    benchmark::DoNotOptimize(workload(n));
  }
}
BENCHMARK(BM_Direct)->Arg(1000)->Arg(5000)->Arg(20000);

static void BM_LRU_Cached_Cold(benchmark::State &state) {
  auto cache = make_lru_cache<uint64_t, uint64_t>(1024, workload);
  const uint64_t n = static_cast<uint64_t>(state.range(0));
  for (auto _ : state) {
    state.PauseTiming();
    auto cold = make_lru_cache<uint64_t, uint64_t>(1024, workload);
    state.ResumeTiming();
    benchmark::DoNotOptimize(cold(n));
  }
}
BENCHMARK(BM_LRU_Cached_Cold)->Arg(1000)->Arg(5000)->Arg(20000);

static void BM_LRU_Cached_Hot(benchmark::State &state) {
  auto cache = make_lru_cache<uint64_t, uint64_t>(1024, workload);
  const uint64_t n = static_cast<uint64_t>(state.range(0));
  benchmark::DoNotOptimize(cache(n));
  for (auto _ : state) {
    benchmark::DoNotOptimize(cache(n));
  }
}
BENCHMARK(BM_LRU_Cached_Hot)->Arg(1000)->Arg(5000)->Arg(20000);

static void BM_Unlimited_Hot(benchmark::State &state) {
  auto cache = make_cache<uint64_t, uint64_t>(workload);
  const uint64_t n = static_cast<uint64_t>(state.range(0));
  benchmark::DoNotOptimize(cache(n));
  for (auto _ : state) {
    benchmark::DoNotOptimize(cache(n));
  }
}
BENCHMARK(BM_Unlimited_Hot)->Arg(1000)->Arg(5000)->Arg(20000);

BENCHMARK_MAIN();
