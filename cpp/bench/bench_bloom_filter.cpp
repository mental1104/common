#include <benchmark/benchmark.h>
#include "mental1104/bloom_filter.h"

#include <string>
#include <vector>

namespace {

std::vector<std::string> make_keys(std::size_t count, std::size_t offset = 0) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    keys.emplace_back("key_" + std::to_string(offset + i));
  }
  return keys;
}

} // namespace

// 构造后立即批量插入，评估插入吞吐
static void BM_BloomFilter_Insert(benchmark::State &state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  auto keys = make_keys(n);

  for (auto _ : state) {
    BloomFilter bf(n, 0.01);
    for (const auto &key : keys) {
      bf.insert(key);
    }
    benchmark::DoNotOptimize(bf.contains(keys.front()));
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(keys.size()));
}

// 预先填充后，仅测查询命中/未命中的路径
static void BM_BloomFilter_Query(benchmark::State &state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  auto keys = make_keys(n);
  auto misses = make_keys(n, n + 1024);

  BloomFilter bf(n * 2, 0.01);
  for (const auto &key : keys) {
    bf.insert(key);
  }

  const std::size_t total = keys.size() + misses.size();
  for (auto _ : state) {
    for (const auto &key : keys) {
      benchmark::DoNotOptimize(bf.contains(key));
    }
    for (const auto &key : misses) {
      benchmark::DoNotOptimize(bf.contains(key));
    }
  }

  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(total));
}

BENCHMARK(BM_BloomFilter_Insert)->Arg(1000)->Arg(10000)->Arg(50000);
BENCHMARK(BM_BloomFilter_Query)->Arg(1000)->Arg(10000)->Arg(50000);

BENCHMARK_MAIN();
