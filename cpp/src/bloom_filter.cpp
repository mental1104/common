#include "mental1104/bloom_filter.h"

namespace mental1104 {
namespace detail {

/// 根据元素数和误判率计算位数及哈希次数，不依赖调用方键或哈希类型。
/// @param expected_count 预期元素数，0 沿用最小存储设置。
/// @param false_positive_rate 目标误判率，沿用原公式的输入契约。
/// @return Bloom filter 的存储参数。
BloomFilterParameters bloom_filter_parameters(std::size_t expected_count,
                                              double false_positive_rate) {
  if (expected_count == 0) {
    return {1, 1};
  }
  const double ln2 = std::log(2.0);
  const auto bit_count = static_cast<std::size_t>(std::ceil(
      -(static_cast<double>(expected_count) * std::log(false_positive_rate)) /
      (ln2 * ln2)));
  const auto hash_count = static_cast<std::size_t>(std::ceil(
      (static_cast<double>(bit_count) / static_cast<double>(expected_count)) *
      ln2));
  return {bit_count, hash_count == 0 ? 1 : hash_count};
}

} // namespace detail
} // namespace mental1104
