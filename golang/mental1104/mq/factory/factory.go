package factory

import (
	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	kafkamq "github.com/mental1104/common/golang/mental1104/mq/kafka"
	pulsarmq "github.com/mental1104/common/golang/mental1104/mq/pulsar"
)

func NewProducer(config commonmq.ProducerConfig) (*commonmq.Producer, error) {
	if config.Backend == nil {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", "", "backend config must not be nil", nil)
	}
	var backend commonmq.ProducerBackend
	var err error
	switch value := config.Backend.(type) {
	case kafkamq.Config:
		backend, err = kafkamq.NewProducerBackend(config, value)
	case *kafkamq.Config:
		if value == nil { err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", commonmq.BackendKafka, "backend config must not be nil", nil) } else { backend, err = kafkamq.NewProducerBackend(config, *value) }
	case pulsarmq.Config:
		backend, err = pulsarmq.NewProducerBackend(config, value)
	case *pulsarmq.Config:
		if value == nil { err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", commonmq.BackendPulsar, "backend config must not be nil", nil) } else { backend, err = pulsarmq.NewProducerBackend(config, *value) }
	default:
		err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create producer", config.Backend.BackendType(), "unsupported backend config", nil)
	}
	if err != nil { return nil, err }
	return commonmq.NewProducer(backend)
}

func NewConsumer(config commonmq.ConsumerConfig) (*commonmq.Consumer, error) {
	if config.Backend == nil {
		return nil, commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", "", "backend config must not be nil", nil)
	}
	var backend commonmq.ConsumerBackend
	var err error
	switch value := config.Backend.(type) {
	case kafkamq.Config:
		backend, err = kafkamq.NewConsumerBackend(config, value)
	case *kafkamq.Config:
		if value == nil { err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", commonmq.BackendKafka, "backend config must not be nil", nil) } else { backend, err = kafkamq.NewConsumerBackend(config, *value) }
	case pulsarmq.Config:
		backend, err = pulsarmq.NewConsumerBackend(config, value)
	case *pulsarmq.Config:
		if value == nil { err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", commonmq.BackendPulsar, "backend config must not be nil", nil) } else { backend, err = pulsarmq.NewConsumerBackend(config, *value) }
	default:
		err = commonmq.NewError(commonmq.ErrorInvalidConfig, "create consumer", config.Backend.BackendType(), "unsupported backend config", nil)
	}
	if err != nil { return nil, err }
	return commonmq.NewConsumer(backend)
}
