# Go 消息队列

## 类别

消息队列 producer / consumer 抽象，以及 Kafka、Pulsar 数据面适配器。

## 公共 API

- 公共包：`github.com/mental1104/common/golang/mental1104/mq`
- 主要类型：`AbstractProducer`、`AbstractConsumer`、`AbstractMessageQueue`
- 发送回调：`SendResult`、`SendCallback`
- Kafka：`github.com/mental1104/common/golang/mental1104/mq/kafka`
- Pulsar：`github.com/mental1104/common/golang/mental1104/mq/pulsar`

公共语义与 `python/mental1104/mq` 对齐：producer 提供 `Send`、`SendAsync`、`Close`；consumer 提供 `Receive`、`Acknowledge`、`NegativeAcknowledge`、`Unsubscribe`、`Resubscribe`、`Close`。

`SendAsync` 接受调用方回调。消息被接受后，回调在成功或失败时恰好执行一次。回调可能运行在客户端或库创建的 goroutine 中，调用方必须自行保证线程安全。

## Kafka 最小示例

```go
package main

import (
    "context"
    "log"

    commonmq "github.com/mental1104/common/golang/mental1104/mq"
    kafkamq "github.com/mental1104/common/golang/mental1104/mq/kafka"
)

func main() {
    queue, err := kafkamq.NewMessageQueue(kafkamq.Config{
        Brokers: []string{"127.0.0.1:9092"},
    })
    if err != nil {
        log.Fatal(err)
    }
    defer queue.Close()

    producer, err := queue.CreateProducer(
        context.Background(), "tenant", "namespace", "events", nil, true,
    )
    if err != nil {
        log.Fatal(err)
    }
    defer producer.Close()

    if err := producer.Send(context.Background(), []byte("sync")); err != nil {
        log.Fatal(err)
    }
    if err := producer.SendAsync(
        context.Background(),
        map[string]any{"kind": "async"},
        func(result commonmq.SendResult) {
            if result.Err != nil {
                log.Printf("send failed: %v", result.Err)
            }
        },
    ); err != nil {
        log.Fatal(err)
    }
}
```

Kafka topic 由非空的 `tenant`、`namespace`、`topic` 使用 `.` 拼接。Kafka consumer 的 nack 通过关闭并重建同一 consumer group reader 实现，未提交消息由 broker 重新分配；这比 Pulsar 原生 nack 更重，也不保证立即由同一实例重新收到。

## Pulsar 最小示例

```go
package main

import (
    "context"
    "log"

    pulsargo "github.com/apache/pulsar-client-go/pulsar"
    commonmq "github.com/mental1104/common/golang/mental1104/mq"
    pulsarmq "github.com/mental1104/common/golang/mental1104/mq/pulsar"
)

func main() {
    queue, err := pulsarmq.NewMessageQueue(pulsarmq.Config{
        ClientOptions: pulsargo.ClientOptions{URL: "pulsar://127.0.0.1:6650"},
    })
    if err != nil {
        log.Fatal(err)
    }
    defer queue.Close()

    producer, err := queue.CreateProducer(
        context.Background(), "tenant", "namespace", "events", nil, true,
    )
    if err != nil {
        log.Fatal(err)
    }
    defer producer.Close()

    if err := producer.SendAsync(
        context.Background(),
        []byte("async"),
        func(result commonmq.SendResult) {
            log.Printf("message_id=%s err=%v", result.MessageID, result.Err)
        },
    ); err != nil {
        log.Fatal(err)
    }
}
```

Pulsar topic 固定为 `persistent://<tenant>/<namespace>/<topic>`。默认订阅类型为 `pulsar.Shared`，可通过 `ConsumerOptions.SubscriptionType` 传入其他 `pulsar.SubscriptionType`。

## 记录编码

- `nil`：空 payload；
- `[]byte`：复制后发送；
- `string`：UTF-8 字节；
- 其他值：使用 `encoding/json` 编码。

## 限制与兼容性

- Go 1.22+。
- Kafka 使用 `github.com/segmentio/kafka-go`。
- Pulsar 使用 `github.com/apache/pulsar-client-go/pulsar`。
- schema 参数保留为公共扩展位；当前适配器发送原始 payload，不管理 schema registry。
- 不提供事务、exactly-once、自动重试、DLQ、指标或 admin API。
- `Close` 幂等；producer 会等待已经接受的异步发送完成，再关闭底层客户端。
