#include <algorithm>
#include <cctype>
#include <gtest/gtest.h>
#include <random>
#include <string>

#include "mental1104/random.h"

namespace {

bool is_hex_string(const std::string &s) {
  return std::all_of(s.begin(), s.end(),
                     [](unsigned char c) { return std::isxdigit(c); });
}

} // namespace

// 恒定输出的伪随机引擎，用于验证模板参数生效。
struct ConstEngine {
  using result_type = uint64_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return 1; } // max 必须大于 min 以通过 uniform_int_distribution 的要求
  explicit ConstEngine(uint64_t seed = 0) { (void)seed; }
  result_type operator()() { return 0; }
};

TEST(RandomTest, DefaultHexNotEmpty) {
  auto s = mental1104::random_hex<>();
  EXPECT_FALSE(s.empty());
}

TEST(RandomTest, CustomEngineDeterministic) {
  auto s = mental1104::random_hex<ConstEngine>(2);
  EXPECT_EQ("00", s); // ConstEngine 始终返回 0，两个块拼成 "00"

  auto key = mental1104::key_with_random_suffix<ConstEngine>("prefix", 1);
  EXPECT_EQ("prefix:0", key);
}

TEST(RandomTest, KeyWithRandomSuffixKeepsPrefix) {
  const std::string prefix = "base";
  auto key = mental1104::key_with_random_suffix(prefix);
  EXPECT_EQ(0u, key.find(prefix + ":"));
  std::string suffix = key.substr(prefix.size() + 1);
  EXPECT_FALSE(suffix.empty());
}

TEST(RandomTest, StdEngineMinstdRand0) {
  auto hex = mental1104::random_hex<std::minstd_rand0>(1);
  EXPECT_FALSE(hex.empty());
  EXPECT_TRUE(std::all_of(hex.begin(), hex.end(),
                          [](unsigned char c) { return std::isxdigit(c); }));

  auto key = mental1104::key_with_random_suffix<std::minstd_rand0>("pref", 2);
  EXPECT_EQ(0u, key.find("pref:"));
  EXPECT_GT(key.size(), std::string("pref:").size());
}

TEST(RandomTest, StdEngineMinstdRand) {
  auto hex = mental1104::random_hex<std::minstd_rand>(1);
  EXPECT_FALSE(hex.empty());
  EXPECT_TRUE(is_hex_string(hex));
}

TEST(RandomTest, StdEngineMt19937) {
  auto hex = mental1104::random_hex<std::mt19937>(2);
  EXPECT_FALSE(hex.empty());
  EXPECT_TRUE(is_hex_string(hex));
}

TEST(RandomTest, StdEngineKnuthB) {
  auto hex = mental1104::random_hex<std::knuth_b>(1);
  EXPECT_FALSE(hex.empty());
  EXPECT_TRUE(is_hex_string(hex));
}

TEST(RandomTest, StdEngineRanlux24Base) {
  auto hex = mental1104::random_hex<std::ranlux24_base>(1);
  EXPECT_FALSE(hex.empty());
  EXPECT_TRUE(is_hex_string(hex));
}

TEST(RandomTest, StdEngineRanlux48Base) {
  auto hex = mental1104::random_hex<std::ranlux48_base>(1);
  EXPECT_FALSE(hex.empty());
  EXPECT_TRUE(is_hex_string(hex));
}
