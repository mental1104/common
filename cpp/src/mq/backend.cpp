#include "mental1104/mq/backend.h"
namespace mental1104 {
namespace mq {
ReceiveResult ReceiveResult::success(const BackendMessage &v) {
  ReceiveResult r;
  r.ok = true;
  r.value = v;
  return r;
}
ReceiveResult ReceiveResult::failure(const MQError &e) {
  ReceiveResult r;
  r.ok = false;
  r.error = e;
  return r;
}
} // namespace mq
} // namespace mental1104
