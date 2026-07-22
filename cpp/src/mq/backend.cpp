#include "mental1104/mq/backend.h"

namespace mental1104 {
namespace mq {

/// 创建包含 BackendMessage 副本的成功接收结果。
ReceiveResult ReceiveResult::success(const BackendMessage &value) {
  ReceiveResult result;
  result.ok = true;
  result.value = value;
  return result;
}

/// 创建包含统一错误的失败接收结果。
ReceiveResult ReceiveResult::failure(const MQError &error) {
  ReceiveResult result;
  result.ok = false;
  result.error = error;
  return result;
}

} // namespace mq
} // namespace mental1104
