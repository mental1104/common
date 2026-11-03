#include "mental1104/random.h"

namespace mental1104 {
namespace detail {

/// 混合单调时钟与随机设备，为每线程引擎提供种子。
/// @return 混合后的 64 位种子；不承诺密码学安全性。
uint64_t mix_seed() {
  return static_cast<uint64_t>(
             std::chrono::steady_clock::now().time_since_epoch().count()) ^
         (static_cast<uint64_t>(std::random_device{}()) << 1);
}

} // namespace detail
} // namespace mental1104
