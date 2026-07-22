package pulsar_test

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
	pulsarmq "github.com/mental1104/common/golang/mental1104/mq/pulsar"
)

// requirePulsarConfig 从环境变量构造真实 Pulsar 配置。
// 优先使用 PULSAR_SERVICE_URL；否则组合 Python 测试沿用的 PULSAR_HOST 和
// PULSAR_BROKER_PORT。缺少配置时使用 t.Skipf 标记跳过。
func requirePulsarConfig(t *testing.T) pulsarmq.Config {
	t.Helper()
	serviceURL := strings.TrimSpace(os.Getenv("PULSAR_SERVICE_URL"))
	if serviceURL == "" {
		host := strings.TrimSpace(os.Getenv("PULSAR_HOST"))
		port := strings.TrimSpace(os.Getenv("PULSAR_BROKER_PORT"))
		if host == "" || port == "" {
			t.Skipf("跳过 Pulsar 集成测试：需要 PULSAR_SERVICE_URL，或同时设置 PULSAR_HOST/PULSAR_BROKER_PORT")
		}
		serviceURL = "pulsar://" + net.JoinHostPort(host, port)
	}
	return pulsarmq.Config{
		ServiceURL:          serviceURL,
		AuthenticationToken: os.Getenv("PULSAR_TOKEN"),
		ConnectionTimeout:   5 * time.Second,
		OperationTimeout:    10 * time.Second,
		CloseTimeout:        10 * time.Second,
		SendTimeout:         10 * time.Second,
		NackRedeliveryDelay: time.Second,
	}
}

// pulsarTestName 生成当前测试独占的 topic 或 subscription 名称。
func pulsarTestName(t *testing.T, suffix string) string {
	t.Helper()
	name := strings.NewReplacer("/", "-", " ", "-").Replace(strings.ToLower(t.Name()))
	return "common-mq-" + name + "-" + suffix + "-" + time.Now().UTC().Format("20060102150405.000000000")
}

// closePulsarProducer 在有限时间内关闭 Producer，并报告清理错误。
func closePulsarProducer(t *testing.T, producer *commonmq.Producer) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := producer.Close(ctx); err != nil {
		t.Errorf("close Pulsar producer: %v", err)
	}
}

// closePulsarConsumer 在有限时间内停止并关闭 Consumer，并报告清理错误。
func closePulsarConsumer(t *testing.T, consumer *commonmq.Consumer) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := consumer.Close(ctx); err != nil {
		t.Errorf("close Pulsar consumer: %v", err)
	}
}

// TestPulsarBridgeWithRealBroker 通过真实 Pulsar 连接验证同步发送、异步 callback、消费与幂等关闭。
func TestPulsarBridgeWithRealBroker(t *testing.T) {
	backend := requirePulsarConfig(t)
	topic := commonmq.Topic{Tenant: "public", Namespace: "default", Name: pulsarTestName(t, "topic")}

	producer, err := factory.NewProducer(commonmq.ProducerConfig{Topic: topic, Backend: backend})
	if err != nil {
		t.Fatalf("create Pulsar producer: %v", err)
	}
	t.Cleanup(func() {
		// Cleanup 是真实 Client/Producer 的最终关闭方。
		closePulsarProducer(t, producer)
	})

	consumer, err := factory.NewConsumer(commonmq.ConsumerConfig{
		Topic:            topic,
		Subscription:     pulsarTestName(t, "subscription"),
		SubscriptionType: commonmq.SubscriptionShared,
		Backend:          backend,
	})
	if err != nil {
		t.Fatalf("create Pulsar consumer: %v", err)
	}
	t.Cleanup(func() {
		// Consumer 关闭会取消 receive 并释放真实 Pulsar Client。
		closePulsarConsumer(t, consumer)
	})

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	received := make(chan string, 2)
	if err := consumer.Start(ctx, func(_ context.Context, message commonmq.Message) (commonmq.ConsumeAction, error) {
		// handler 串行处理并返回 ack，Pulsar broker 随后删除订阅中的已确认消息。
		received <- string(message.Payload)
		return commonmq.ConsumeAcknowledge, nil
	}); err != nil {
		t.Fatalf("start Pulsar consumer: %v", err)
	}

	if _, err := producer.Send(ctx, commonmq.NewMessage([]byte("sync-message"))); err != nil {
		t.Fatalf("send Pulsar sync message: %v", err)
	}
	select {
	case payload := <-received:
		if payload != "sync-message" {
			t.Fatalf("unexpected Pulsar sync payload %q", payload)
		}
	case <-ctx.Done():
		t.Fatalf("receive Pulsar sync message: %v", ctx.Err())
	}

	var callbackCount atomic.Int32
	callbackDone := make(chan commonmq.SendResult, 1)
	if err := producer.Async().SendAsync(ctx, commonmq.NewMessage([]byte("async-message")), func(result commonmq.SendResult) {
		// callback 由 backend 独立 goroutine 调用；测试通过缓冲 channel 观察最终结果。
		callbackCount.Add(1)
		callbackDone <- result
	}); err != nil {
		t.Fatalf("submit Pulsar async message: %v", err)
	}
	select {
	case result := <-callbackDone:
		if !result.OK() || result.MessageID == "" {
			t.Fatalf("unexpected Pulsar async result: %+v", result)
		}
	case <-ctx.Done():
		t.Fatalf("wait Pulsar async callback: %v", ctx.Err())
	}
	select {
	case payload := <-received:
		if payload != "async-message" {
			t.Fatalf("unexpected Pulsar async payload %q", payload)
		}
	case <-ctx.Done():
		t.Fatalf("receive Pulsar async message: %v", ctx.Err())
	}
	if callbackCount.Load() != 1 {
		t.Fatalf("Pulsar callback count=%d, want 1", callbackCount.Load())
	}

	if err := consumer.Stop(ctx); err != nil {
		t.Fatalf("stop Pulsar consumer: %v", err)
	}
	if err := producer.Close(ctx); err != nil {
		t.Fatalf("close Pulsar producer: %v", err)
	}
	if err := producer.Close(ctx); err != nil {
		t.Fatalf("close Pulsar producer again: %v", err)
	}
}
