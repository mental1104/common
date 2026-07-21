# C++ 消息队列

## 类别与路径

- 类别：消息队列 producer / consumer。
- 公共抽象：`mental1104/mq/abstract_message_queue.h`。
- transport 适配层：`mental1104/mq/transport.h`。
- Kafka：`mental1104/mq/kafka.h`。
- Pulsar：`mental1104/mq/pulsar.h`。

## 公共能力

API 与 `python/mental1104/mq` 的核心语义对齐：

- `AbstractProducer::send`、`send_async`、`close`；
- `AbstractConsumer::receive`、`acknowledge`、`negative_acknowledge`、`unsubscribe`、`resubscribe`、`close`；
- `AbstractMessageQueue::create_producer`、`create_consumer`、`close`。

`send_async` 接受下游传入的 `SendCallback`。每个已被 transport 接受的异步发送，在成功或失败时回调一次；`Producer::close` 会等待所有已接受回调完成。回调可能在 native client 线程或内部 poll 线程执行，调用方必须自行保证线程安全，且回调异常会被隔离。

## Kafka 最小示例

```cpp
#include "mental1104/mq/kafka.h"

#include <iostream>

int main() {
  mental1104::mq::Options config;
  config["bootstrap.servers"] = "127.0.0.1:9092";
  mental1104::mq::KafkaMessageQueue queue(config);
  std::shared_ptr<mental1104::mq::AbstractProducer> producer =
      queue.create_producer("tenant", "namespace", "events");

  producer->send(mental1104::mq::make_record("sync"));
  producer->send_async(
      mental1104::mq::make_record("async"),
      [](const mental1104::mq::SendResult &result) {
        if (!result.ok) {
          std::cerr << result.error << "\n";
        }
      });
  producer->close();
}
```

Kafka topic 由非空的 `tenant`、`namespace`、`topic` 使用 `.` 拼接。consumer ack 使用同步提交；nack 将当前分区 seek 回消息 offset。

## Pulsar 最小示例

```cpp
#include "mental1104/mq/pulsar.h"

#include <iostream>

int main() {
  mental1104::mq::Options config;
  config["service.url"] = "pulsar://127.0.0.1:6650";
  mental1104::mq::PulsarMessageQueue queue(config);
  std::shared_ptr<mental1104::mq::AbstractProducer> producer =
      queue.create_producer("tenant", "namespace", "events");

  producer->send_async(
      mental1104::mq::make_record("async"),
      [](const mental1104::mq::SendResult &result) {
        std::cout << result.message_id << " " << result.error << "\n";
      });
  producer->close();
}
```

Pulsar topic 固定为 `persistent://<tenant>/<namespace>/<topic>`；consumer 使用原生 ack、negative acknowledge 与 unsubscribe。

## 构建与兼容性

- 公共抽象和 transport 层支持 C++11、14、17、20、23。
- CMake 会可选检测 librdkafka C++ 与 pulsar-client-cpp。
- 缺少 native client 时，`kafka_available()` / `pulsar_available()` 返回 `false`，默认核心库仍可构建；创建对应后端会抛出包含安装提示的 `std::runtime_error`。
- 当前 pulsar-client-cpp 适配器仅在 C++17+ 且检测到库时启用，不提高仓库最低 C++ 标准。
- 不提供事务、exactly-once、自动重试、DLQ、指标、schema registry 或 admin API。
