#include "mental1104/bloom_filter.h"

#include <cmath>
#include <iostream>

BloomFilter::BloomFilter(size_t n, double p) {
  // 计算所需的位数组大小 m
  m = std::ceil(-(n * std::log(p)) / (std::log(2) * std::log(2)));
  // 计算哈希函数个数 k
  k = std::ceil((m / n) * std::log(2));

  bit_array.resize(m, false);
  // std::cout << "Bloom Filter 初始化: 位数组大小 = " << m
  //          << ", 哈希函数个数 = " << k << std::endl;
}

size_t BloomFilter::hash(const std::string &key, size_t seed) const {
  return (hash_fn(key) ^ (seed * 0x5bd1e995)) % m;
}

void BloomFilter::insert(const std::string &key) {
  for (size_t i = 0; i < k; ++i) {
    size_t index = hash(key, i);
    bit_array[index] = true;
  }
}

bool BloomFilter::contains(const std::string &key) const {
  for (size_t i = 0; i < k; ++i) {
    size_t index = hash(key, i);
    if (!bit_array[index])
      return false; // 只要有一个 bit 为 0，则一定不存在
  }
  return true; // 可能存在（存在误判）
}
