// bridge_demo 使用公共 Bridge API 连接真实 Kafka 或 Pulsar，演示同步发送、
// 异步 callback 和 Consumer handler。程序不直接操作任何 SDK Client。
package main

import (
	"context"
	"errors"
	"fmt"
	"net"
	"os"
	"strings"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	"github.com/mental1104/common/golang/mental1104/mq/factory"
	kafkamq "github.com/mental1104/common/golang/mental1104/mq/kafka"
	pulsarmq "github.com/mental1104/common/golang/mental1104/mq/pulsar"
)

// demoConfig 保存一次真实后端演示所需的公共配置。
type demoConfig struct {
	topic          commonmq.Topic
	producerConfig commonmq.ProducerConfig
	consumerConfig commonmq.ConsumerConfig
}

// kafkaBrokersFromEnv 读取 Kafka bootstrap 地址。
// KAFKA_BOOTSTRAP_SERVERS 可包含逗号分隔地址；否则组合 KAFKA_ADVERTISED_HOST
// 与 KAFKA_EXTERNAL_PORT。缺少配置时返回说明性错误。
func kafkaBrokersFromEnv() ([]string, error) {
	if raw := strings.TrimSpace(os.Getenv("KAFKA_BOOTSTRAP_SERVERS")); raw != "" {
		parts := strings.Split(raw, ",")
		brokers := make([]string, 0, len(parts))
		for _, part := range parts {
			if value := strings.TrimSpace(part); value != "" {
				brokers = append(brokers, value)
			}
		}
		if len(brokers) > 0 {
			return brokers, nil
		}
	}
	host := strings.TrimSpace(os.Getenv("KAFKA_ADVERTISED_HOST"))
	port := strings.TrimSpace(os.Getenv("KAFKA_EXTERNAL_PORT"))
	if host == "" || port == "" {
		return nil, errors.New("Kafka 需要 KAFKA_BOOTSTRAP_SERVERS，或 KAFKA_ADVERTISED_HOST/KAFKA_EXTERNAL_PORT")
	}
	return []string{net.JoinHostPort(host, port)}, nil
}

// pulsarURLFromEnv 读取 Pulsar service URL。
// 优先使用 PULSAR_SERVICE_URL；否则组合 PULSAR_HOST 和 PULSAR_BROKER_PORT。
func pulsarURLFromEnv() (string, error) {
	if serviceURL := strings.TrimSpace(os.Getenv("PULSAR_SERVICE_URL")); serviceURL != "" {
		return serviceURL, nil
	}
	host := strings.TrimSpace(os.Getenv("PULSAR_HOST"))
	port := strings.TrimSpace(os.Getenv("PULSAR_BROKER_PORT"))
	if host == "" || port == "" {
		return "", errors.New("Pulsar 需要 PULSAR_SERVICE_URL，或 PULSAR_HOST/PULSAR_BROKER_PORT")
	}
	return "pulsar://" + net.JoinHostPort(host, port), nil
}

// buildDemoConfig 根据 MQ_BACKEND 创建 Kafka 或 Pulsar 类型安全配置。
// MQ_BACKEND 只接受 kafka 或 pulsar；topic/subscription 使用时间戳隔离本次运行。
func buildDemoConfig() (demoConfig, error) {
	unique := time.Now().UTC().Format("20060102150405.000000000")
	switch strings.ToLower(strings.TrimSpace(os.Getenv("MQ_BACKEND"))) {
	case "kafka":
		brokers, err := kafkaBrokersFromEnv()
		if err != nil {
			return demoConfig{}, err
		}
		topic := commonmq.Topic{Tenant: "common", Namespace: "demo", Name: "bridge-" + unique}
		backend := kafkamq.Config{
			Brokers:                brokers,
			DialTimeout:            5 * time.Second,
			ReadTimeout:            10 * time.Second,
			WriteTimeout:           10 * time.Second,
			MaxWait:                time.Second,
			AckMode:                kafkamq.AckAll,
			AllowAutoTopicCreation: true,
		}
		return demoConfig{
			topic:          topic,
			producerConfig: commonmq.ProducerConfig{Topic: topic, Backend: backend},
			consumerConfig: commonmq.ConsumerConfig{Topic: topic, Subscription: "bridge-demo-" + unique, SubscriptionType: commonmq.SubscriptionShared, Backend: backend},
		}, nil
	case "pulsar":
		serviceURL, err := pulsarURLFromEnv()
		if err != nil {
			return demoConfig{}, err
		}
		topic := commonmq.Topic{Tenant: "public", Namespace: "default", Name: "bridge-" + unique}
		backend := pulsarmq.Config{
			ServiceURL:          serviceURL,
			AuthenticationToken: os.Getenv("PULSAR_TOKEN"),
			ConnectionTimeout:   5 * time.Second,
			OperationTimeout:    10 * time.Second,
			CloseTimeout:        10 * time.Second,
			SendTimeout:         10 * time.Second,
		}
		return demoConfig{
			topic:          topic,
			producerConfig: commonmq.ProducerConfig{Topic: topic, Backend: backend},
			consumerConfig: commonmq.ConsumerConfig{Topic: topic, Subscription: "bridge-demo-" + unique, SubscriptionType: commonmq.SubscriptionShared, Backend: backend},
		}, nil
	default:
		return demoConfig{}, errors.New("请设置 MQ_BACKEND=kafka 或 MQ_BACKEND=pulsar")
	}
}

// runDemo 运行一次真实 Producer/Consumer 闭环。
// ctx 控制所有网络操作；函数返回前会按 Consumer -> Producer 顺序关闭资源。
func runDemo(ctx context.Context, config demoConfig) error {
	producer, err := factory.NewProducer(config.producerConfig)
	if err != nil {
		return fmt.Errorf("create producer: %w", err)
	}
	defer func() {
		// defer 是 Producer 的最终关闭方；主流程错误时也释放连接。
		_ = producer.Close(context.Background())
	}()

	consumer, err := factory.NewConsumer(config.consumerConfig)
	if err != nil {
		return fmt.Errorf("create consumer: %w", err)
	}
	defer func() {
		// Consumer 先关闭，确保 handler 不再访问正在销毁的演示状态。
		_ = consumer.Close(context.Background())
	}()

	received := make(chan string, 2)
	if err := consumer.Start(ctx, func(_ context.Context, message commonmq.Message) (commonmq.ConsumeAction, error) {
		// handler 在 Bridge goroutine 中串行执行，返回 ack 提交消息。
		received <- string(message.Payload)
		return commonmq.ConsumeAcknowledge, nil
	}); err != nil {
		return fmt.Errorf("start consumer: %w", err)
	}

	syncResult, err := producer.Send(ctx, commonmq.NewMessage([]byte("sync-message")))
	if err != nil {
		return fmt.Errorf("send sync message: %w", err)
	}
	fmt.Printf("sync: id=%s partition=%v\n", syncResult.MessageID, syncResult.Partition)

	callbackDone := make(chan commonmq.SendResult, 1)
	if err := producer.Async().SendAsync(ctx, commonmq.NewMessage([]byte("async-message")), func(result commonmq.SendResult) {
		// callback 只负责转交最终结果；缓冲 channel 无需由 callback 关闭。
		callbackDone <- result
	}); err != nil {
		return fmt.Errorf("submit async message: %w", err)
	}
	select {
	case result := <-callbackDone:
		if !result.OK() {
			return fmt.Errorf("async callback: %w", result.Err)
		}
		fmt.Printf("async callback: id=%s partition=%v\n", result.MessageID, result.Partition)
	case <-ctx.Done():
		return ctx.Err()
	}

	for index := 0; index < 2; index++ {
		select {
		case payload := <-received:
			fmt.Printf("consumer: %s\n", payload)
		case <-ctx.Done():
			return ctx.Err()
		}
	}
	return consumer.Stop(ctx)
}

// main 解析环境配置并运行 30 秒超时的真实消息队列示例。
func main() {
	config, err := buildDemoConfig()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	if err := runDemo(ctx, config); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
