#include "mental1104/bloom_filter.h"
#include <gtest/gtest.h>

// 测试 BloomFilter 的初始化
TEST(BloomFilterTest, Initialization) {
  BloomFilter bf(1000, 0.01);

  // 验证位数组大小和哈希函数个数
  EXPECT_GT(bf.getM(), 1000);
  EXPECT_GT(bf.getK(), 0);
  EXPECT_EQ(bf.getBitArray().size(), bf.getM());
}

// 测试插入单个元素
TEST(BloomFilterTest, InsertSingleElement) {
  BloomFilter bf(1000, 0.01);

  std::string key = "test_key";
  bf.insert(key);

  // 插入后应该能查询到
  EXPECT_TRUE(bf.contains(key));
}

// 测试多个元素插入
TEST(BloomFilterTest, InsertMultipleElements) {
  BloomFilter bf(1000, 0.01);

  std::vector<std::string> keys = {"apple", "banana", "cherry"};
  for (const auto &key : keys) {
    bf.insert(key);
  }

  // 插入的元素应该能查询到
  for (const auto &key : keys) {
    EXPECT_TRUE(bf.contains(key));
  }
}

// 测试未插入的元素
TEST(BloomFilterTest, CheckNonInsertedElement) {
  BloomFilter bf(1000, 0.01);

  std::string key = "not_inserted";
  EXPECT_FALSE(bf.contains(key));
}

// 测试误判率（大致验证）
TEST(BloomFilterTest, FalsePositiveRate) {
  BloomFilter bf(1000, 0.01);

  // 插入 1000 个随机字符串
  for (int i = 0; i < 1000; ++i) {
    bf.insert("key" + std::to_string(i));
  }

  // 查询 1000 个未插入的字符串
  int false_positives = 0;
  for (int i = 1000; i < 2000; ++i) {
    if (bf.contains("key" + std::to_string(i))) {
      false_positives++;
    }
  }

  double actual_fp_rate = static_cast<double>(false_positives) / 1000;
  EXPECT_LE(actual_fp_rate, 0.05); // 误判率应低于 5%
}
