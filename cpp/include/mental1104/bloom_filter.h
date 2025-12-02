#ifndef __MENTAL1104_BLOOM_FILTER
#define __MENTAL1104_BLOOM_FILTER

#include <cmath>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// 通用模板版布隆过滤器
template <typename Key, typename Hash = std::hash<Key>> class BasicBloomFilter {
public:
  using key_type = Key;
  using hash_type = Hash;

protected:
  std::size_t m;               // 位数组大小
  std::size_t k;               // 哈希函数个数
  std::vector<bool> bit_array; // 位数组
  hash_type hash_fn;           // 针对 Key 的哈希函数对象

  // 使用一个基础哈希值 + seed 混合出 k 个下标
  std::size_t hash(const key_type &key, std::size_t seed) const {
    std::size_t h = hash_fn(key);
    // 和你原先的写法保持一致：XOR + 一个固定常数
    return (h ^ (seed * 0x5bd1e995u)) % m;
  }

public:
  // 构造函数：n 预期元素数，p 允许的误判率
  explicit BasicBloomFilter(std::size_t n, double p,
                            const hash_type &hash = hash_type())
      : m(0), k(0), bit_array(), hash_fn(hash) {
    if (n == 0) {
      // 极端情况兜底，避免除 0
      m = 1;
      k = 1;
      bit_array.assign(m, false);
      return;
    }

    const double ln2 = std::log(2.0);
    // m = - (n * ln p) / (ln2^2)
    m = static_cast<std::size_t>(
        std::ceil(-(static_cast<double>(n) * std::log(p)) / (ln2 * ln2)));

    // k = (m / n) * ln2
    k = static_cast<std::size_t>(
        std::ceil((static_cast<double>(m) / static_cast<double>(n)) * ln2));

    if (k == 0) {
      k = 1; // 保险：至少一个哈希函数
    }

    bit_array.assign(m, false);
  }

  // 插入元素
  void insert(const key_type &key) {
    for (std::size_t i = 0; i < k; ++i) {
      std::size_t index = hash(key, i);
      bit_array[index] = true;
    }
  }

  // 查询元素是否“可能存在”
  bool contains(const key_type &key) const {
    for (std::size_t i = 0; i < k; ++i) {
      std::size_t index = hash(key, i);
      if (!bit_array[index]) {
        return false; // 只要有一个 bit 为 0，则一定不存在
      }
    }
    return true; // 可能存在（存在误判）
  }

  // 以下接口保持和你原来的一致，用于测试 / 调试
  std::size_t getM() const { return m; }
  std::size_t getK() const { return k; }
  const std::vector<bool> &getBitArray() const { return bit_array; }
};

// 对外仍然暴露原来的 BloomFilter（字符串版）
// 原有所有代码、单测继续用 BloomFilter 就行
using BloomFilter = BasicBloomFilter<std::string, std::hash<std::string>>;

#include <mutex>
#include <shared_mutex> // C++17
#include <utility>

// 粗粒度“大锁”版本：一把 shared_mutex 包整个 BloomFilter
// 1. 读（contains）用 shared_lock，可并发
// 2. 写（insert）用 unique_lock，串行
//
// 模板版包装：可用于任意 BasicBloomFilter<Key, Hash>
template <typename Key, typename Hash = std::hash<Key>>
class ThreadSafeBloomFilter {
public:
  using key_type = Key;
  using hash_type = Hash;
  using underlying_type = BasicBloomFilter<Key, Hash>;

  // 构造：保持和 BasicBloomFilter 一致
  explicit ThreadSafeBloomFilter(std::size_t n, double p,
                                 const hash_type &hash = hash_type())
      : bf_(n, p, hash) {}

  // 禁止拷贝，允许移动（根据你需要也可以自行放开）
  ThreadSafeBloomFilter(const ThreadSafeBloomFilter &) = delete;
  ThreadSafeBloomFilter &operator=(const ThreadSafeBloomFilter &) = delete;

  ThreadSafeBloomFilter(ThreadSafeBloomFilter &&other) noexcept {
    std::unique_lock lk_other(other.mutex_);
    bf_ = std::move(other.bf_);
  }

  ThreadSafeBloomFilter &operator=(ThreadSafeBloomFilter &&other) noexcept {
    if (this == &other)
      return *this;
    // 避免死锁：按地址排序上锁
    ThreadSafeBloomFilter *first = this < &other ? this : &other;
    ThreadSafeBloomFilter *second = this < &other ? &other : this;
    std::unique_lock lk1(first->mutex_);
    std::unique_lock lk2(second->mutex_);
    bf_ = std::move(other.bf_);
    return *this;
  }

  // 插入：独占锁
  void insert(const key_type &key) {
    std::unique_lock lk(mutex_);
    bf_.insert(key);
  }

  // 查询：共享锁
  bool contains(const key_type &key) const {
    std::shared_lock lk(mutex_);
    return bf_.contains(key);
  }

  // ==== 监控/调试接口：加读锁后转调底层 ====

  std::size_t getM() const {
    std::shared_lock lk(mutex_);
    return bf_.getM();
  }

  std::size_t getK() const {
    std::shared_lock lk(mutex_);
    return bf_.getK();
  }

  const std::vector<bool> getBitArraySnapshot() const {
    std::shared_lock lk(mutex_);
    // 返回一份拷贝，避免把内部引用暴露出去引起 data race
    return bf_.getBitArray();
  }

  // 如果你确实需要访问底层对象，可以提供一个带锁执行的 helper
  template <typename Fn> auto with_lock(Fn &&fn) {
    std::unique_lock lk(mutex_);
    return fn(bf_);
  }

  template <typename Fn> auto with_shared_lock(Fn &&fn) const {
    std::shared_lock lk(mutex_);
    return fn(bf_);
  }

private:
  mutable std::shared_mutex mutex_;
  underlying_type bf_;
};

// 常用：字符串版线程安全 BloomFilter
// 用法：ThreadSafeStringBloomFilter bf(n, p); 和原来 BloomFilter 一样用
// insert/contains
using ThreadSafeStringBloomFilter =
    ThreadSafeBloomFilter<std::string, std::hash<std::string>>;

// 如果你想保持命名风格，也可以再别名一个更短的名字：
// using TSBloomFilter = ThreadSafeStringBloomFilter;

#endif // __MENTAL1104_BLOOM_FILTER
