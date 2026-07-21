package kafka

import (
	"context"
	"sync/atomic"
	"testing"
	"time"

	commonmq "github.com/mental1104/common/golang/mental1104/mq"
	kafkago "github.com/segmentio/kafka-go"
)

type fakeWriter struct{ message kafkago.Message; closed atomic.Int32 }
func (w *fakeWriter) WriteMessages(_ context.Context, messages ...kafkago.Message) error { w.message = messages[0]; return nil }
func (w *fakeWriter) Close() error { w.closed.Add(1); return nil }

type fakeReader struct{ message kafkago.Message; commits atomic.Int32; closed atomic.Int32 }
func (r *fakeReader) FetchMessage(context.Context) (kafkago.Message,error){return r.message,nil}
func (r *fakeReader) CommitMessages(context.Context,...kafkago.Message)error{r.commits.Add(1);return nil}
func (r *fakeReader) Close()error{r.closed.Add(1);return nil}

func TestProducerBackendConvertsDomainMessageAndCallbackMayClose(t *testing.T){
	w:=&fakeWriter{};backend:=newProducerBackend(w);producer,_:=commonmq.NewProducer(backend)
	partition:=3;message:=commonmq.Message{Key:[]byte("key"),Payload:[]byte("payload"),Headers:commonmq.MessageHeaders{"trace":"1"},Partition:&partition}
	if _,err:=producer.Send(context.Background(),message);err!=nil{t.Fatal(err)}
	if string(w.message.Key)!="key"||string(w.message.Value)!="payload"||w.message.Partition!=3||len(w.message.Headers)!=1{t.Fatalf("native message=%+v",w.message)}
	done:=make(chan error,1)
	if err:=producer.Async().SendAsync(context.Background(),message,func(commonmq.SendResult){done<-producer.Close(context.Background())});err!=nil{t.Fatal(err)}
	select{case err:=<-done:if err!=nil{t.Fatal(err)};case<-time.After(time.Second):t.Fatal("callback deadlocked while closing producer")}
	if w.closed.Load()!=1{t.Fatalf("close count=%d",w.closed.Load())}
}

func TestConsumerBackendHidesNativeMessageBehindReceipt(t *testing.T){
	r:=&fakeReader{message:kafkago.Message{Topic:"events",Partition:2,Offset:9,Key:[]byte("k"),Value:[]byte("v"),Headers:[]kafkago.Header{{Key:"trace",Value:[]byte("1")}}}}
	backend:=newConsumerBackend("events","sub",func()(reader,error){return r,nil},r)
	message,err:=backend.Receive(context.Background());if err!=nil{t.Fatal(err)}
	if message.ReceiptID==""||message.Message.ID!="events/2/9"||string(message.Message.Payload)!="v"||message.Message.Headers["trace"]!="1"{t.Fatalf("message=%+v",message)}
	if err:=backend.Acknowledge(context.Background(),message.ReceiptID);err!=nil{t.Fatal(err)}
	if r.commits.Load()!=1{t.Fatalf("commits=%d",r.commits.Load())}
}
