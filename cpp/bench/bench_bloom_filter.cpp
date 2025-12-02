#include "mental1104/bloom_filter.h"
#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <unordered_map>
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

void simulate_io_miss(
    std::chrono::microseconds cost = std::chrono::microseconds(50)) {
  std::this_thread::sleep_for(cost);
}

std::size_t bloom_mem_bytes(const BloomFilter &bf) {
  // 位数组大小 m 是 bit 数，换算为字节（向上取整）
  return (bf.getM() + 7) / 8;
}

std::size_t
unordered_map_mem_bytes(const std::unordered_map<std::string, int> &mp) {
  // 粗略估算：桶指针 + 节点 + 字符存储
  std::size_t bytes = mp.bucket_count() * sizeof(void *);
  bytes +=
      mp.size() * (sizeof(std::pair<const std::string, int>) + sizeof(void *));
  for (const auto &kv : mp) {
    bytes += kv.first.capacity();
  }
  return bytes;
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

// 对照场景：未使用布隆过滤，直接查询 unordered_map，未命中时模拟 IO
static void BM_MapBackend_Mixed(benchmark::State &state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0)); // 元素规模
  const std::size_t hit_ratio =
      static_cast<std::size_t>(state.range(1)); // 命中百分比 [0,100]

  auto present = make_keys(n);
  auto misses = make_keys(n, n + 1024);

  std::unordered_map<std::string, int> store;
  store.reserve(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    store.emplace(present[i], static_cast<int>(i));
  }

  const std::size_t hit_count = std::min<std::size_t>(n, n * hit_ratio / 100);
  const std::size_t miss_count = n - hit_count;
  std::vector<std::string> queries;
  queries.reserve(hit_count + miss_count);
  queries.insert(queries.end(), present.begin(), present.begin() + hit_count);
  queries.insert(queries.end(), misses.begin(), misses.begin() + miss_count);

  for (auto _ : state) {
    for (const auto &key : queries) {
      auto it = store.find(key);
      if (it == store.end()) {
        simulate_io_miss();
      }
      benchmark::DoNotOptimize(it);
    }
  }

  state.counters["hit_rate"] = benchmark::Counter(
      static_cast<double>(hit_count) / static_cast<double>(queries.size()),
      benchmark::Counter::kAvgThreads);
  state.counters["mem_bytes"] =
      static_cast<double>(unordered_map_mem_bytes(store));
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(queries.size()));
}

// 布隆过滤守护的查询：先查布隆，过滤绝大部分未命中请求，再进入 unordered_map/IO
static void BM_BloomGuardedBackend_Mixed(benchmark::State &state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0)); // 元素规模
  const std::size_t hit_ratio =
      static_cast<std::size_t>(state.range(1)); // 命中百分比 [0,100]

  auto present = make_keys(n);
  auto misses = make_keys(n, n + 1024);

  BloomFilter bf(n, 0.01);
  for (const auto &key : present) {
    bf.insert(key);
  }

  std::unordered_map<std::string, int> store;
  store.reserve(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    store.emplace(present[i], static_cast<int>(i));
  }

  const std::size_t hit_count = std::min<std::size_t>(n, n * hit_ratio / 100);
  const std::size_t miss_count = n - hit_count;
  std::vector<std::string> queries;
  queries.reserve(hit_count + miss_count);
  queries.insert(queries.end(), present.begin(), present.begin() + hit_count);
  queries.insert(queries.end(), misses.begin(), misses.begin() + miss_count);

  std::size_t skipped = 0;
  std::size_t false_positive_io = 0;
  for (auto _ : state) {
    for (const auto &key : queries) {
      if (!bf.contains(key)) {
        ++skipped;
        continue; // 直接丢弃未命中的请求
      }
      auto it = store.find(key);
      if (it == store.end()) {
        ++false_positive_io;
        simulate_io_miss();
      }
      benchmark::DoNotOptimize(it);
    }
  }

  const std::size_t total_queries = queries.size();
  state.counters["hit_rate"] = benchmark::Counter(
      static_cast<double>(hit_count) / static_cast<double>(total_queries),
      benchmark::Counter::kAvgThreads);
  state.counters["skipped_rate"] = benchmark::Counter(
      static_cast<double>(skipped) /
          static_cast<double>(total_queries * state.iterations()),
      benchmark::Counter::kAvgThreads);
  state.counters["false_positive_io"] =
      benchmark::Counter(static_cast<double>(false_positive_io) /
                             static_cast<double>(state.iterations()),
                         benchmark::Counter::kAvgThreads);
  state.counters["mem_bytes"] =
      static_cast<double>(unordered_map_mem_bytes(store) + bloom_mem_bytes(bf));
  state.counters["mem_bloom_bytes"] = static_cast<double>(bloom_mem_bytes(bf));
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(total_queries));
}

BENCHMARK(BM_BloomFilter_Insert)->Arg(1000)->Arg(10000)->Arg(50000);
BENCHMARK(BM_BloomFilter_Query)->Arg(1000)->Arg(10000)->Arg(50000);
BENCHMARK(BM_MapBackend_Mixed)
    ->Args({1000, 10})
    ->Args({1000, 50})
    ->Args({1000, 90})
    ->Args({5000, 10})
    ->Args({5000, 50})
    ->Args({5000, 90});
BENCHMARK(BM_BloomGuardedBackend_Mixed)
    ->Args({1000, 10})
    ->Args({1000, 50})
    ->Args({1000, 90})
    ->Args({5000, 10})
    ->Args({5000, 50})
    ->Args({5000, 90});

BENCHMARK_MAIN();
