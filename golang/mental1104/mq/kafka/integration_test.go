package kafka_test

import (
	"context"
	"net"
	"os"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	"github.com/mental1104/common/golang/mental1104/mq/factory"
	kafkamq "github.com/mental1104/common/golang/mental1104/mq/kafka"
)

// requireKafkaBrokers 从环境变量读取真实 Kafka bootstrap 地址。
// 优先使用逗号分隔的 KAFKA_BOOTSTRAP_SERVERS；否则组合 Python 测试沿用的
// KAFKA_ADVERTISED_HOST 和 KAFKA_EXTERNAL_PORT。缺少配置时使用 t.Skipf 标记跳过。
func requireKafkaBrokers(t *testing.T) []string {
	t.Helper()
	if raw := strings.TrimSpace(os.Getenv("KAFKA_BOOTSTRAP_SERVERS")); raw != "" {
		parts := strings.Split(raw, ",")
		brokers := make([]string, 0, len(parts))
		for _, part := range parts {
			if value := strings.TrimSpace(part); value != "" {
				brokers = append(brokers, value)
			}
		}
		if len(brokers) > 0 {
			return brokers
		}
	}
	host := strings.TrimSpace(os.Getenv("KAFKA_ADVERTISED_HOST"))
	port := strings.TrimSpace(os.Getenv("KAFKA_EXTERNAL_PORT"))
	if host == "" || port == "" {
		t.Skipf("跳过 Kafka 集成测试：需要 KAFKA_BOOTSTRAP_SERVERS，或同时设置 KAFKA_ADVERTISED_HOST/KAFKA_EXTERNAL_PORT")
	}
	return []string{net.JoinHostPort(host, port)}
}

// kafkaTestName 生成当前测试独占的 topic 或 consumer group 名称，避免并行运行互相消费。
func kafkaTestName(t *testing.T, suffix string) string {
	t.Helper()
	name := strings.NewReplacer("/", "-", " ", "-").Replace(strings.ToLower(t.Name()))
	return "common-mq-" + name + "-" + suffix + "-" + time.Now().UTC().Format("20060102150405.000000000")
}

// closeKafkaProducer 在有限时间内关闭 Producer，并把清理失败报告给测试。
func closeKafkaProducer(t *testing.T, producer *commonmq.Producer) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := producer.Close(ctx); err != nil {
		t.Errorf("close Kafka producer: %v", err)
	}
}

// closeKafkaConsumer 在有限时间内停止并关闭 Consumer，并把清理失败报告给测试。
func closeKafkaConsumer(t *testing.T, consumer *commonmq.Consumer) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := consumer.Close(ctx); err != nil {
		t.Errorf("close Kafka consumer: %v", err)
	}
}

// TestKafkaBridgeWithRealBroker 通过真实 Kafka 连接验证同步发送、异步 callback、消费与幂等关闭。
func TestKafkaBridgeWithRealBroker(t *testing.T) {
	brokers := requireKafkaBrokers(t)
	topic := commonmq.Topic{Tenant: "common", Namespace: "integration", Name: kafkaTestName(t, "topic")}
	backend := kafkamq.Config{
		Brokers:                brokers,
		DialTimeout:            5 * time.Second,
		ReadTimeout:            10 * time.Second,
		WriteTimeout:           10 * time.Second,
		MaxWait:                time.Second,
		AckMode:                kafkamq.AckAll,
		AllowAutoTopicCreation: true,
	}

	producer, err := factory.NewProducer(commonmq.ProducerConfig{Topic: topic, Backend: backend})
	if err != nil {
		t.Fatalf("create Kafka producer: %v", err)
	}
	t.Cleanup(func() {
		// Cleanup 是资源关闭方；测试提前失败时也必须释放真实网络连接。
		closeKafkaProducer(t, producer)
	})

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	syncMessage := commonmq.NewMessage([]byte("sync-message"))
	if _, err := producer.Send(ctx, syncMessage); err != nil {
		t.Fatalf("send Kafka sync message: %v", err)
	}

	consumer, err := factory.NewConsumer(commonmq.ConsumerConfig{
		Topic:            topic,
		Subscription:     kafkaTestName(t, "group"),
		SubscriptionType: commonmq.SubscriptionShared,
		Backend:          backend,
	})
	if err != nil {
		t.Fatalf("create Kafka consumer: %v", err)
	}
	t.Cleanup(func() {
		// Consumer 必须在测试结束时停止 receive goroutine 并关闭 Reader。
		closeKafkaConsumer(t, consumer)
	})

	received := make(chan string, 2)
	if err := consumer.Start(ctx, func(_ context.Context, message commonmq.Message) (commonmq.ConsumeAction, error) {
		// handler 运行在 Consumer Bridge 管理的单一 goroutine 中；返回 ack 提交 offset。
		received <- string(message.Payload)
		return commonmq.ConsumeAcknowledge, nil
	}); err != nil {
		t.Fatalf("start Kafka consumer: %v", err)
	}

	select {
	case payload := <-received:
		if payload != "sync-message" {
			t.Fatalf("unexpected Kafka sync payload %q", payload)
		}
	case <-ctx.Done():
		t.Fatalf("receive Kafka sync message: %v", ctx.Err())
	}

	var callbackCount atomic.Int32
	callbackDone := make(chan commonmq.SendResult, 1)
	if err := producer.Async().SendAsync(ctx, commonmq.NewMessage([]byte("async-message")), func(result commonmq.SendResult) {
		// callback 由 backend 独立 goroutine 调用；channel 由测试接收方持有且不关闭。
		callbackCount.Add(1)
		callbackDone <- result
	}); err != nil {
		t.Fatalf("submit Kafka async message: %v", err)
	}

	select {
	case result := <-callbackDone:
		if !result.OK() {
			t.Fatalf("Kafka async callback failed: %v", result.Err)
		}
	case <-ctx.Done():
		t.Fatalf("wait Kafka async callback: %v", ctx.Err())
	}
	select {
	case payload := <-received:
		if payload != "async-message" {
			t.Fatalf("unexpected Kafka async payload %q", payload)
		}
	case <-ctx.Done():
		t.Fatalf("receive Kafka async message: %v", ctx.Err())
	}
	if callbackCount.Load() != 1 {
		t.Fatalf("Kafka callback count=%d, want 1", callbackCount.Load())
	}

	if err := consumer.Stop(ctx); err != nil {
		t.Fatalf("stop Kafka consumer: %v", err)
	}
	if err := producer.Close(ctx); err != nil {
		t.Fatalf("close Kafka producer: %v", err)
	}
	if err := producer.Close(ctx); err != nil {
		t.Fatalf("close Kafka producer again: %v", err)
	}
}
