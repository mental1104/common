# C++ 测试资产

这里维护只面向测试代码复用的 C++ 资产。它们与 `cpp/include/mental1104` 下的普通公共能力分开构建，不应被生产源码依赖。

## MQTT

### `MqttPeerFixture`

- **类别：** MQTT 测试资产
- **类型：** 可组合测试夹具
- **定义位置：** `cpp/testing/include/mental1104/testing/mqtt_peer_fixture.h`
- **包含：** `#include "mental1104/testing/mqtt_peer_fixture.h"`
- **链接：** `mental1104_testing_mqtt`
- **用途：** 基于 `mqtt::Client` 提供 publish / subscribe / unsubscribe 原语，以及消息等待和顺序 request-response 测试便利接口。

**最小用法：**

```cpp
#include "mental1104/testing/mqtt_peer_fixture.h"

#include <chrono>

int main() {
  mental1104::mqtt::ClientOptions options;
  options.host = "127.0.0.1";
  options.port = 1883;
  options.client_id = "fixture-demo";

  mental1104::testing::MqttPeerFixture peer(options);
  if (!peer.start().ok)
    return 1;

  auto result = peer.request(
      "demo/request",
      "demo/response",
      "ping",
      std::chrono::seconds(2));

  return result.status.ok ? 0 : 2;
}
```

**示例结果：**

```text
无标准输出；broker 上有对端把 demo/request 的请求应答到 demo/response 时退出码为 0，否则为非 0。
```

**备注：**

- `MqttPeerFixture` 不继承 `::testing::Test`，业务测试可以按需组合多个实例。
- `request()` 明确接收 request topic 和 response topic；内部仅组合 subscribe、publish、wait_message 原语。
- `request()` 不理解 JSON、tid 或 request_id，也不承担同一 response topic 的并发关联。需要这类语义时应在业务测试中基于原语显式实现。
- fixture 不进入 `mental1104` 普通库 target，生产代码无需链接测试资产。
