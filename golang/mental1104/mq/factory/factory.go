// Package factory 根据类型安全的后端配置创建公共 Producer 和 Consumer Bridge。
// 本包只负责选择实现，不承载消息发送、消费循环或 SDK 生命周期逻辑。
package factory

import (
	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	kafkamq "github.com/mental1104/common/golang/mental1104/mq/kafka"
	pulsarmq "github.com/mental1104/common/golang/mental1104/mq/pulsar"
)

// NewProducer 根据 config.Backend 创建 Kafka 或 Pulsar ProducerBackend，
// 再用公共 Bridge 包装。Backend 可使用值或非 nil 指针；其他类型返回 ErrorInvalidConfig。
func NewProducer(config commonmq.ProducerConfig) (*commonmq.Producer, error) {
	if config.Backend == nil {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", "", "backend config must not be nil", nil)
	}
	var (
		backend commonmq.ProducerBackend
		err     error
	)
	// Factory 只在这里选择具体实现，避免业务代码散落 backend 类型判断。
	switch value := config.Backend.(type) {
	case kafkamq.Config:
		backend, err = kafkamq.NewProducerBackend(config, value)
	case *kafkamq.Config:
		if value == nil {
			err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", commonmq.BackendKafka, "backend config must not be nil", nil)
		} else {
			backend, err = kafkamq.NewProducerBackend(config, *value)
		}
	case pulsarmq.Config:
		backend, err = pulsarmq.NewProducerBackend(config, value)
	case *pulsarmq.Config:
		if value == nil {
			err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", commonmq.BackendPulsar, "backend config must not be nil", nil)
		} else {
			backend, err = pulsarmq.NewProducerBackend(config, *value)
		}
	default:
		err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", config.Backend.BackendType(), "unsupported backend config", nil)
	}
	if err != nil {
		return nil, err
	}
	return commonmq.NewProducer(backend)
}

// NewConsumer 根据 config.Backend 创建 Kafka 或 Pulsar ConsumerBackend，
// 再用公共 Bridge 包装。Backend 可使用值或非 nil 指针；其他类型返回 ErrorInvalidConfig。
func NewConsumer(config commonmq.ConsumerConfig) (*commonmq.Consumer, error) {
	if config.Backend == nil {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", "", "backend config must not be nil", nil)
	}
	var (
		backend commonmq.ConsumerBackend
		err     error
	)
	// Producer 和 Consumer 分别选择小接口，避免形成同时拥有全部能力的上帝 backend。
	switch value := config.Backend.(type) {
	case kafkamq.Config:
		backend, err = kafkamq.NewConsumerBackend(config, value)
	case *kafkamq.Config:
		if value == nil {
			err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", commonmq.BackendKafka, "backend config must not be nil", nil)
		} else {
			backend, err = kafkamq.NewConsumerBackend(config, *value)
		}
	case pulsarmq.Config:
		backend, err = pulsarmq.NewConsumerBackend(config, value)
	case *pulsarmq.Config:
		if value == nil {
			err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", commonmq.BackendPulsar, "backend config must not be nil", nil)
		} else {
			backend, err = pulsarmq.NewConsumerBackend(config, *value)
		}
	default:
		err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", config.Backend.BackendType(), "unsupported backend config", nil)
	}
	if err != nil {
		return nil, err
	}
	return commonmq.NewConsumer(backend)
}
