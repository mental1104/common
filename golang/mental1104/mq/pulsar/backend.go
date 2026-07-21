package pulsar

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	pulsargo "github.com/apache/pulsar-client-go/pulsar"
	commonmq "github.com/mental1104/common/golang/mental1104/mq"
)

type Config struct {
	ServiceURL          string
	AuthenticationToken string
	ConnectionTimeout   time.Duration
	OperationTimeout    time.Duration
	CloseTimeout        time.Duration
	SendTimeout         time.Duration
	MaxPendingMessages  int
	ReceiverQueueSize   int
	NackRedeliveryDelay time.Duration
}

func (Config) BackendType() commonmq.BackendType { return commonmq.BackendPulsar }

type messageIDClient interface { String() string; PartitionIdx() int32 }
type messageClient interface {
	Topic() string
	Properties() map[string]string
	Payload() []byte
	ID() pulsargo.MessageID
	Key() string
}
type producerClient interface {
	Send(context.Context, *pulsargo.ProducerMessage) (pulsargo.MessageID, error)
	SendAsync(context.Context, *pulsargo.ProducerMessage, func(pulsargo.MessageID, *pulsargo.ProducerMessage, error))
	FlushWithCtx(context.Context) error
	Close()
}
type consumerClient interface {
	Receive(context.Context) (messageClient, error)
	Ack(messageClient) error
	Nack(messageClient)
	Unsubscribe() error
	Close()
}
type client interface {
	CreateProducer(pulsargo.ProducerOptions) (producerClient, error)
	Subscribe(pulsargo.ConsumerOptions) (consumerClient, error)
	Close()
}

type nativeClient struct{ pulsargo.Client }
type nativeConsumer struct{ pulsargo.Consumer }
func (c nativeClient) CreateProducer(o pulsargo.ProducerOptions) (producerClient,error) { return c.Client.CreateProducer(o) }
func (c nativeClient) Subscribe(o pulsargo.ConsumerOptions) (consumerClient,error) { v,e:=c.Client.Subscribe(o); if e!=nil{return nil,e}; return nativeConsumer{Consumer:v},nil }
func (c nativeConsumer) Receive(ctx context.Context) (messageClient,error) { return c.Consumer.Receive(ctx) }
func (c nativeConsumer) Ack(m messageClient) error { v,ok:=m.(pulsargo.Message); if !ok{return commonmq.ErrInvalidMessage}; return c.Consumer.Ack(v) }
func (c nativeConsumer) Nack(m messageClient) { if v,ok:=m.(pulsargo.Message);ok{c.Consumer.Nack(v)} }

func newClient(config Config) (client,error) {
	if config.ServiceURL=="" { return nil,commonmq.NewError(commonmq.ErrorInvalidConfig,"create pulsar client",commonmq.BackendPulsar,"service URL must not be empty",nil) }
	o:=pulsargo.ClientOptions{URL:config.ServiceURL,ConnectionTimeout:config.ConnectionTimeout,OperationTimeout:config.OperationTimeout}
	if config.AuthenticationToken!="" { o.Authentication=pulsargo.NewAuthenticationToken(config.AuthenticationToken) }
	v,err:=pulsargo.NewClient(o); if err!=nil{return nil,commonmq.NormalizeError(err,commonmq.ErrorBackend,"create pulsar client",commonmq.BackendPulsar)}
	return nativeClient{Client:v},nil
}

type producerBackend struct {
	client client
	producer producerClient
	closeDelay time.Duration
	mu sync.Mutex
	closed bool
	pending int
	idle chan struct{}
	closeOnce sync.Once
	closeDone chan struct{}
	closeErr error
}

func NewProducerBackend(config commonmq.ProducerConfig, backend Config) (commonmq.ProducerBackend,error) {
	topic,err:=commonmq.BuildPulsarTopic(config.Topic); if err!=nil{return nil,err}
	c,err:=newClient(backend); if err!=nil{return nil,err}
	o:=pulsargo.ProducerOptions{Topic:topic,DisableBatching:config.DisableBatching,SendTimeout:backend.SendTimeout,MaxPendingMessages:backend.MaxPendingMessages,DisableBlockIfQueueFull:false}
	p,err:=c.CreateProducer(o); if err!=nil{c.Close();return nil,commonmq.NormalizeError(err,commonmq.ErrorBackend,"create pulsar producer",commonmq.BackendPulsar)}
	return newProducerBackend(c,p,backend.CloseTimeout),nil
}

func newProducerBackend(c client,p producerClient,timeout time.Duration)*producerBackend{
	idle:=make(chan struct{});close(idle);if timeout<=0{timeout=10*time.Second}
	return &producerBackend{client:c,producer:p,closeDelay:timeout,idle:idle,closeDone:make(chan struct{})}
}
func (b *producerBackend) begin() error { b.mu.Lock();defer b.mu.Unlock();if b.closed{return commonmq.ErrClosed};if b.pending==0{b.idle=make(chan struct{})};b.pending++;return nil }
func (b *producerBackend) finish(){b.mu.Lock();b.pending--;if b.pending==0{close(b.idle)};b.mu.Unlock()}
func pulsarMessage(m commonmq.Message)*pulsargo.ProducerMessage{p:=make(map[string]string,len(m.Headers));for k,v:=range m.Headers{p[k]=v};return &pulsargo.ProducerMessage{Payload:append([]byte(nil),m.Payload...),Key:string(m.Key),Properties:p}}
func resultFromID(id messageIDClient,err error)commonmq.SendResult{r:=commonmq.SendResult{Err:err};if id!=nil{r.MessageID=id.String();p:=int(id.PartitionIdx());r.Partition=&p};return r}
func (b *producerBackend) Send(ctx context.Context,m commonmq.Message)(commonmq.SendResult,error){if err:=b.begin();err!=nil{return commonmq.SendResult{Err:err},err};defer b.finish();id,err:=b.producer.Send(ctx,pulsarMessage(m));if err!=nil{err=commonmq.NormalizeError(err,commonmq.ErrorBackend,"pulsar send",commonmq.BackendPulsar);return resultFromID(id,err),err};return resultFromID(id,nil),nil}
func (b *producerBackend) SendAsync(ctx context.Context,m commonmq.Message,cb commonmq.DeliveryCallback)error{
	if err:=b.begin();err!=nil{return err}
	b.producer.SendAsync(ctx,pulsarMessage(m),func(id pulsargo.MessageID,_ *pulsargo.ProducerMessage,err error){if err!=nil{err=commonmq.NormalizeError(err,commonmq.ErrorBackend,"pulsar async send",commonmq.BackendPulsar)};b.finish();if cb!=nil{r:=resultFromID(id,err);go func(){defer func(){_=recover()}();cb(r)}()}});return nil
}
func (b *producerBackend) Close(ctx context.Context)error{b.closeOnce.Do(func(){b.mu.Lock();b.closed=true;idle:=b.idle;b.mu.Unlock();go func(){<-idle;fctx,cancel:=context.WithTimeout(context.Background(),b.closeDelay);e:=b.producer.FlushWithCtx(fctx);cancel();b.producer.Close();b.client.Close();b.closeErr=commonmq.NormalizeError(e,commonmq.ErrorBackend,"close pulsar producer",commonmq.BackendPulsar);close(b.closeDone)}()});select{case<-b.closeDone:return b.closeErr;case<-ctx.Done():return commonmq.NormalizeError(ctx.Err(),commonmq.ErrorCanceled,"close pulsar producer",commonmq.BackendPulsar)}}

type consumerBackend struct {client client;options pulsargo.ConsumerOptions;mu sync.Mutex;closed bool;consumer consumerClient;receipts map[string]messageClient;sequence atomic.Uint64}
func NewConsumerBackend(config commonmq.ConsumerConfig,backend Config)(commonmq.ConsumerBackend,error){if config.Subscription==""{return nil,commonmq.NewError(commonmq.ErrorInvalidConfig,"create pulsar consumer",commonmq.BackendPulsar,"subscription must not be empty",nil)};topic,err:=commonmq.BuildPulsarTopic(config.Topic);if err!=nil{return nil,err};c,err:=newClient(backend);if err!=nil{return nil,err};o:=pulsargo.ConsumerOptions{Topic:topic,SubscriptionName:config.Subscription,Type:pulsarSubscriptionType(config.SubscriptionType),ReceiverQueueSize:backend.ReceiverQueueSize,NackRedeliveryDelay:backend.NackRedeliveryDelay};v,err:=c.Subscribe(o);if err!=nil{c.Close();return nil,commonmq.NormalizeError(err,commonmq.ErrorBackend,"create pulsar consumer",commonmq.BackendPulsar)};return newConsumerBackend(c,v,o),nil}
func newConsumerBackend(c client,v consumerClient,o pulsargo.ConsumerOptions)*consumerBackend{return &consumerBackend{client:c,options:o,consumer:v,receipts:make(map[string]messageClient)}}
func pulsarSubscriptionType(v commonmq.SubscriptionType)pulsargo.SubscriptionType{switch v{case commonmq.SubscriptionExclusive:return pulsargo.Exclusive;case commonmq.SubscriptionFailover:return pulsargo.Failover;case commonmq.SubscriptionKeyShared:return pulsargo.KeyShared;default:return pulsargo.Shared}}
func (b *consumerBackend) Receive(ctx context.Context)(commonmq.BackendMessage,error){b.mu.Lock();if b.closed||b.consumer==nil{b.mu.Unlock();return commonmq.BackendMessage{},commonmq.ErrClosed};c:=b.consumer;b.mu.Unlock();v,err:=c.Receive(ctx);if err!=nil{return commonmq.BackendMessage{},err};id:=v.ID();rid:=fmt.Sprintf("%s:%d",id.String(),b.sequence.Add(1));h:=make(commonmq.MessageHeaders,len(v.Properties()));for k,x:=range v.Properties(){h[k]=x};p:=int(id.PartitionIdx());m:=commonmq.Message{Topic:v.Topic(),Key:[]byte(v.Key()),Payload:append([]byte(nil),v.Payload()...),Headers:h,Partition:&p,ID:id.String()};b.mu.Lock();if b.closed{b.mu.Unlock();return commonmq.BackendMessage{},commonmq.ErrClosed};b.receipts[rid]=v;b.mu.Unlock();return commonmq.NewBackendMessage(m,rid),nil}
func (b *consumerBackend) receipt(id string,remove bool)(consumerClient,messageClient,error){b.mu.Lock();defer b.mu.Unlock();if b.closed||b.consumer==nil{return nil,nil,commonmq.ErrClosed};m,ok:=b.receipts[id];if !ok{return nil,nil,commonmq.ErrInvalidMessage};if remove{delete(b.receipts,id)};return b.consumer,m,nil}
func (b *consumerBackend) Acknowledge(_ context.Context,id string)error{c,m,err:=b.receipt(id,false);if err!=nil{return err};if err:=c.Ack(m);err!=nil{return commonmq.NormalizeError(err,commonmq.ErrorBackend,"pulsar acknowledge",commonmq.BackendPulsar)};b.mu.Lock();delete(b.receipts,id);b.mu.Unlock();return nil}
func (b *consumerBackend) NegativeAcknowledge(_ context.Context,id string)error{c,m,err:=b.receipt(id,true);if err!=nil{return err};c.Nack(m);return nil}
func (b *consumerBackend) Unsubscribe(context.Context)error{b.mu.Lock();if b.closed{b.mu.Unlock();return commonmq.ErrClosed};c:=b.consumer;b.consumer=nil;b.receipts=make(map[string]messageClient);b.mu.Unlock();if c==nil{return nil};return commonmq.NormalizeError(c.Unsubscribe(),commonmq.ErrorBackend,"unsubscribe pulsar consumer",commonmq.BackendPulsar)}
func (b *consumerBackend) Resubscribe(context.Context)error{b.mu.Lock();if b.closed{b.mu.Unlock();return commonmq.ErrClosed};c:=b.consumer;b.mu.Unlock();if c!=nil{c.Close()};next,err:=b.client.Subscribe(b.options);if err!=nil{return commonmq.NormalizeError(err,commonmq.ErrorBackend,"resubscribe pulsar consumer",commonmq.BackendPulsar)};b.mu.Lock();b.consumer=next;b.receipts=make(map[string]messageClient);b.mu.Unlock();return nil}
func (b *consumerBackend) Close(context.Context)error{b.mu.Lock();if b.closed{b.mu.Unlock();return nil};b.closed=true;c:=b.consumer;b.consumer=nil;b.receipts=nil;b.mu.Unlock();if c!=nil{c.Close()};b.client.Close();return nil}

var _ commonmq.ProducerBackend=(*producerBackend)(nil)
var _ commonmq.ConsumerBackend=(*consumerBackend)(nil)
