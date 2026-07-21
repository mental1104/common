package pulsar

import (
	"context"
	"sync/atomic"
	"testing"
	"time"

	pulsargo "github.com/apache/pulsar-client-go/pulsar"
	commonmq "github.com/mental1104/common/golang/mental1104/mq"
)

type fakeProducer struct{ message *pulsargo.ProducerMessage; closed atomic.Int32 }
func (p *fakeProducer) Send(context.Context,*pulsargo.ProducerMessage)(pulsargo.MessageID,error){return pulsargo.NewMessageID(1,2,0,3),nil}
func (p *fakeProducer) SendAsync(_ context.Context,m *pulsargo.ProducerMessage,cb func(pulsargo.MessageID,*pulsargo.ProducerMessage,error)){p.message=m;go cb(pulsargo.NewMessageID(1,2,0,3),m,nil)}
func (p *fakeProducer) FlushWithCtx(context.Context)error{return nil}
func (p *fakeProducer) Close(){p.closed.Add(1)}

type fakeMessage struct{ payload []byte }
func (m fakeMessage) Topic()string{return "persistent://t/n/events"}
func (m fakeMessage) Properties()map[string]string{return map[string]string{"trace":"1"}}
func (m fakeMessage) Payload()[]byte{return m.payload}
func (m fakeMessage) ID()pulsargo.MessageID{return pulsargo.NewMessageID(1,2,0,3)}
func (m fakeMessage) Key()string{return "key"}

type fakeConsumer struct{ message messageClient; acks,nacks,closed atomic.Int32 }
func (c *fakeConsumer) Receive(context.Context)(messageClient,error){return c.message,nil}
func (c *fakeConsumer) Ack(messageClient)error{c.acks.Add(1);return nil}
func (c *fakeConsumer) Nack(messageClient){c.nacks.Add(1)}
func (c *fakeConsumer) Unsubscribe()error{return nil}
func (c *fakeConsumer) Close(){c.closed.Add(1)}

type fakeClient struct{ producer producerClient; consumer consumerClient; closed atomic.Int32 }
func (c *fakeClient) CreateProducer(pulsargo.ProducerOptions)(producerClient,error){return c.producer,nil}
func (c *fakeClient) Subscribe(pulsargo.ConsumerOptions)(consumerClient,error){return c.consumer,nil}
func (c *fakeClient) Close(){c.closed.Add(1)}

func TestProducerBackendConvertsMessageAndCallbackMayClose(t *testing.T){
	native:=&fakeProducer{};client:=&fakeClient{producer:native};backend:=newProducerBackend(client,native,time.Second);producer,_:=commonmq.NewProducer(backend)
	message:=commonmq.Message{Key:[]byte("key"),Payload:[]byte("payload"),Headers:commonmq.MessageHeaders{"trace":"1"}}
	done:=make(chan error,1)
	if err:=producer.Async().SendAsync(context.Background(),message,func(result commonmq.SendResult){if result.MessageID==""{t.Error("missing message id")};done<-producer.Close(context.Background())});err!=nil{t.Fatal(err)}
	select{case err:=<-done:if err!=nil{t.Fatal(err)};case<-time.After(time.Second):t.Fatal("callback deadlocked while closing producer")}
	if string(native.message.Payload)!="payload"||native.message.Key!="key"||native.message.Properties["trace"]!="1"{t.Fatalf("native message=%+v",native.message)}
}

func TestConsumerBackendUsesInternalReceipt(t *testing.T){
	native:=&fakeConsumer{message:fakeMessage{payload:[]byte("value")}};client:=&fakeClient{consumer:native};backend:=newConsumerBackend(client,native,pulsargo.ConsumerOptions{})
	message,err:=backend.Receive(context.Background());if err!=nil{t.Fatal(err)}
	if message.ReceiptID==""||string(message.Message.Payload)!="value"||message.Message.Headers["trace"]!="1"{t.Fatalf("message=%+v",message)}
	if err:=backend.Acknowledge(context.Background(),message.ReceiptID);err!=nil{t.Fatal(err)}
	if native.acks.Load()!=1{t.Fatalf("acks=%d",native.acks.Load())}
}
