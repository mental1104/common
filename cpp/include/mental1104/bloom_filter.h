#ifndef __MENTAL1104_BLOOM_FILTER
#define __MENTAL1104_BLOOM_FILTER

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

class BloomFilter {
protected:
  size_t m;                       // 位数组大小
  size_t k;                       // 哈希函数个数
  std::vector<bool> bit_array;    // 位数组
  std::hash<std::string> hash_fn; // 标准哈希函数

  // 哈希函数: 使用两个哈希值生成 k 个索引
  size_t hash(const std::string &key, size_t seed) const;

public:
  // 构造函数，允许用户设置误判率 p 和预期存储的元素数 n
  BloomFilter(size_t n, double p);

  // 插入元素
  void insert(const std::string &key);

  // 查询元素是否存在
  bool contains(const std::string &key) const;

public:
  // 公开 `m` 和 `k`，用于测试
  size_t getM() const { return m; }
  size_t getK() const { return k; }
  const std::vector<bool> &getBitArray() const { return bit_array; }
};

#endif
