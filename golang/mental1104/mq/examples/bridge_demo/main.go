package main

import (
	"context"
	"fmt"
	"sync"

	"github.com/mental1104/common/golang/mental1104/mq"
)

type demoProducerBackend struct{}

func (demoProducerBackend) Send(context.Context, mq.Message) (mq.SendResult, error) {
	return mq.SendResult{MessageID: "sync-1"}, nil
}
func (demoProducerBackend) SendAsync(_ context.Context, _ mq.Message, callback mq.DeliveryCallback) error {
	go callback(mq.SendResult{MessageID: "async-1"})
	return nil
}
func (demoProducerBackend) Close(context.Context) error { return nil }

type demoConsumerBackend struct{ once sync.Once }
func (b *demoConsumerBackend) Receive(ctx context.Context) (mq.BackendMessage, error) {
	var message mq.BackendMessage
	delivered := false
	b.once.Do(func() { delivered = true; message = mq.NewBackendMessage(mq.NewMessage([]byte("consumed")), "receipt-1") })
	if delivered { return message, nil }
	<-ctx.Done()
	return mq.BackendMessage{}, ctx.Err()
}
func (*demoConsumerBackend) Acknowledge(context.Context, string) error          { return nil }
func (*demoConsumerBackend) NegativeAcknowledge(context.Context, string) error { return nil }
func (*demoConsumerBackend) Unsubscribe(context.Context) error                 { return nil }
func (*demoConsumerBackend) Resubscribe(context.Context) error                 { return nil }
func (*demoConsumerBackend) Close(context.Context) error                       { return nil }

func main() {
	ctx := context.Background()
	producer, _ := mq.NewProducer(demoProducerBackend{})
	result, _ := producer.Send(ctx, mq.NewMessage([]byte("sync")))
	fmt.Println("sync:", result.MessageID)

	asyncDone := make(chan struct{})
	_ = producer.Async().SendAsync(ctx, mq.NewMessage([]byte("async")), func(result mq.SendResult) {
		fmt.Println("async callback:", result.MessageID)
		close(asyncDone)
	})
	<-asyncDone
	_ = producer.Close(ctx)

	consumer, _ := mq.NewConsumer(&demoConsumerBackend{})
	consumed := make(chan struct{})
	_ = consumer.Start(ctx, func(_ context.Context, message mq.Message) (mq.ConsumeAction, error) {
		fmt.Println("consumer:", string(message.Payload))
		close(consumed)
		return mq.ConsumeAcknowledge, nil
	})
	<-consumed
	_ = consumer.Close(ctx)
}
