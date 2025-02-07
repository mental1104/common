#ifndef __MENTAL1104_BLOOM_FILTER
#define __MENTAL1104_BLOOM_FILTER

#include <iostream>
#include <vector>
#include <cmath>
#include <bitset>
#include <functional>

class BloomFilter {
protected:
    size_t m;   // 位数组大小
    size_t k;   // 哈希函数个数
    std::vector<bool> bit_array; // 位数组
    std::hash<std::string> hash_fn; // 标准哈希函数

    // 哈希函数: 使用两个哈希值生成 k 个索引
    size_t hash(const std::string& key, size_t seed) const {
        return (hash_fn(key) ^ (seed * 0x5bd1e995)) % m;
    }

public:
    // 构造函数，允许用户设置误判率 p 和预期存储的元素数 n
    BloomFilter(size_t n, double p) {
        // 计算所需的位数组大小 m
        m = std::ceil(-(n * std::log(p)) / (std::log(2) * std::log(2)));
        // 计算哈希函数个数 k
        k = std::ceil((m / n) * std::log(2));

        bit_array.resize(m, false);
        std::cout << "Bloom Filter 初始化: 位数组大小 = " << m << ", 哈希函数个数 = " << k << std::endl;
    }

    // 插入元素
    void insert(const std::string& key) {
        for (size_t i = 0; i < k; ++i) {
            size_t index = hash(key, i);
            bit_array[index] = true;
        }
    }

    // 查询元素是否存在
    bool contains(const std::string& key) const {
        for (size_t i = 0; i < k; ++i) {
            size_t index = hash(key, i);
            if (!bit_array[index])
                return false; // 只要有一个 bit 为 0，则一定不存在
        }
        return true; // 可能存在（存在误判）
    }

    public:
    // 公开 `m` 和 `k`，用于测试
    size_t getM() const { return m; }
    size_t getK() const { return k; }
    const std::vector<bool>& getBitArray() const { return bit_array; }

};

#endif