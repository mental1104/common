#include <atomic>
#include <cstddef>
#include <benchmark/benchmark.h>

#include "mental1104/concurrency/mn/mn_coroutine_pool.h"

using mental1104::Task;

namespace {

Task counting_coro(std::atomic<int>& counter, int steps) {
  for (int i = 0; i < steps; ++i) {
    counter.fetch_add(1, std::memory_order_relaxed);
    co_await std::suspend_always{};
  }
  co_return;
}

template <class Pool>
void RunPoolBatch(Pool& pool, int coro_count, int steps) {
  std::atomic<int> counter{0};
  for (int i = 0; i < coro_count; ++i) {
    pool.spawn(counting_coro(counter, steps));
  }
  pool.wait_all();
}

template <class Pool>
void BM_Pool(benchmark::State& state) {
  const int threads    = static_cast<int>(state.range(0));
  const int coro_count = static_cast<int>(state.range(1));
  const int steps      = static_cast<int>(state.range(2));

  for (auto _ : state) {
    state.PauseTiming();
    Pool pool(threads);
    state.ResumeTiming();
    RunPoolBatch(pool, coro_count, steps);
  }
}

}  // namespace

static void ArgsNonBlocking(benchmark::internal::Benchmark* b) {
  b->Args({2, 1000, 2});
  b->Args({4, 2000, 2});
  b->Args({4, 4000, 1});
  b->Args({4, 10000, 3});
  b->Args({8, 20000, 3});
}

#define CONFIGURE_POOL_BENCH(PoolType)                                \
  BENCHMARK_TEMPLATE(BM_Pool, PoolType)                               \
      ->Apply(ArgsNonBlocking)                                        \
      ->Iterations(3)                                                 \
      ->Repetitions(1)                                                \
      ->Unit(benchmark::kMillisecond)

CONFIGURE_POOL_BENCH(mental1104::MnCoroutinePool);

#if defined(M1104_HAS_ASYNC_SIMPLE)
CONFIGURE_POOL_BENCH(mental1104::MnCoroutinePoolAsyncSimple);
#endif

CONFIGURE_POOL_BENCH(mental1104::BoostMnCoroutinePool);

#if defined(M1104_HAS_ASYNC_SIMPLE)
CONFIGURE_POOL_BENCH(mental1104::BoostMnCoroutinePoolAsyncSimple);
#endif

BENCHMARK_MAIN();
