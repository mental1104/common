#pragma once

#include <chrono>
#include <random>
#include <sstream>
#include <string>

namespace mental1104 {
namespace detail {

inline uint64_t mix_seed() {
  return static_cast<uint64_t>(
             std::chrono::steady_clock::now().time_since_epoch().count()) ^
         (static_cast<uint64_t>(std::random_device{}()) << 1);
}

template <typename Engine>
Engine &thread_local_engine() {
  static thread_local Engine engine(mix_seed());
  return engine;
}

} // namespace detail

// 生成十六进制随机串，blocks 表示拼接多少个 64bit 随机块。
template <typename Engine = std::mt19937_64>
inline std::string random_hex(std::size_t blocks = 2) {
  auto &rng = detail::thread_local_engine<Engine>();
  std::uniform_int_distribution<uint64_t> dist;
  std::stringstream ss;
  ss << std::hex;
  for (std::size_t i = 0; i < blocks; ++i) {
    ss << dist(rng);
  }
  return ss.str();
}

// 在前缀后加随机十六进制后缀，便于避免 key 冲突。
template <typename Engine = std::mt19937_64>
inline std::string key_with_random_suffix(const std::string &prefix,
                                          std::size_t blocks = 1) {
  std::stringstream ss;
  ss << prefix << ":" << random_hex<Engine>(blocks);
  return ss.str();
}

} // namespace mental1104

