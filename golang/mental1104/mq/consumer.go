package mq

import (
	"context"
	"errors"
	"sync"
)

// Consumer 通过 ConsumerBackend 提供手动拉取和非阻塞消费循环。
//
// mu 保护运行、关闭和最终错误状态。每个 Consumer 最多运行一个消费 goroutine，
// handler 在该 goroutine 中串行执行；Stop 后允许再次 Start，Close 后不允许重启。
type Consumer struct {
	backend ConsumerBackend

	mu        sync.Mutex
	running   bool
	closed    bool
	cancel    context.CancelFunc
	done      chan struct{}
	lastErr   error
	closeDone chan struct{}
	closeErr  error
	closing   bool
}

// NewConsumer 使用 backend 创建 Consumer。
// backend 不能为 nil，其连接和内部 goroutine 由 Consumer.Close 负责关闭。
func NewConsumer(backend ConsumerBackend) (*Consumer, error) {
	if backend == nil {
		return nil, NewError(ErrorInvalidConfig, "new consumer", "", "consumer backend must not be nil", nil)
	}
	return &Consumer{backend: backend}, nil
}

// Start 非阻塞启动一个串行消费 goroutine。
// ctx 取消、Stop 或 Close 都会终止接收；handler 不能为 nil。重复启动返回
// ErrAlreadyStarted，已经关闭返回 ErrClosed。
func (c *Consumer) Start(ctx context.Context, handler MessageHandler) error {
	if handler == nil {
		return NewError(ErrorInvalidConfig, "start consumer", "", "message handler must not be nil", nil)
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed {
		return ErrClosed
	}
	if c.running {
		return ErrAlreadyStarted
	}
	runCtx, cancel := context.WithCancel(ctx)
	c.running = true
	c.cancel = cancel
	c.done = make(chan struct{})
	c.lastErr = nil
	go c.consumeLoop(runCtx, handler, c.done)
	return nil
}

// consumeLoop 持续接收消息、调用 handler 并执行确认决策。
// done 只由本 goroutine 关闭；退出前会原子更新 running/cancel 状态供 Stop/Close 等待。
func (c *Consumer) consumeLoop(ctx context.Context, handler MessageHandler, done chan struct{}) {
	defer func() {
		c.mu.Lock()
		c.running = false
		c.cancel = nil
		close(done)
		c.mu.Unlock()
	}()

	for {
		backendMessage, err := c.backend.Receive(ctx)
		if err != nil {
			if ctx.Err() != nil || errors.Is(err, context.Canceled) || errors.Is(err, ErrCanceled) || errors.Is(err, ErrClosed) {
				// 主动停止属于正常生命周期，不记录为消费失败。
				return
			}
			if errors.Is(err, context.DeadlineExceeded) || errors.Is(err, ErrTimeout) {
				// 单次 receive 超时不结束 Consumer，下一轮继续等待消息。
				continue
			}
			c.setLastError(NormalizeError(err, ErrorBackend, "receive message", ""))
			return
		}

		message := attachReceipt(backendMessage.Message, backendMessage.ReceiptID)
		action, handlerErr := invokeHandler(ctx, handler, message)
		if handlerErr != nil {
			// handler 失败统一 nack，避免把未处理成功的消息错误提交。
			action = ConsumeNegativeAcknowledge
			c.setLastError(NewError(ErrorHandler, "handle message", "", "", handlerErr))
		}

		switch action {
		case ConsumeAcknowledge:
			if err := c.Acknowledge(ctx, message); err != nil {
				c.setLastError(err)
				return
			}
		case ConsumeNegativeAcknowledge:
			if err := c.NegativeAcknowledge(ctx, message); err != nil {
				c.setLastError(err)
				return
			}
		case ConsumeLeaveUnacknowledged:
			// 调用方明确选择保留未确认状态，Bridge 不做隐式提交。
		default:
			c.setLastError(NewError(ErrorHandler, "handle message", "", "unknown consume action", nil))
			if err := c.NegativeAcknowledge(ctx, message); err != nil {
				c.setLastError(err)
			}
			return
		}
	}
}

// invokeHandler 调用业务 handler，并把 panic 转换为稳定 ErrorHandler。
// message 在调用前深拷贝，handler 可以在当前调用内安全修改其切片和 map。
func invokeHandler(ctx context.Context, handler MessageHandler, message Message) (action ConsumeAction, err error) {
	defer func() {
		if recovered := recover(); recovered != nil {
			err = fmtPanic(recovered)
		}
	}()
	return handler(ctx, CloneMessage(message))
}

// fmtPanic 把任意 panic 值转换成 MQError。
func fmtPanic(value any) error {
	return NewError(ErrorHandler, "message handler panic", "", "handler panicked", errors.New(toString(value)))
}

// toString 为 panic 值生成稳定文本，避免依赖 fmt 对复杂对象的自定义方法。
func toString(value any) string {
	if text, ok := value.(string); ok {
		return text
	}
	return "non-string panic value"
}

// setLastError 保存消费循环最后一次需要由 Stop/Close 返回的错误。
func (c *Consumer) setLastError(err error) {
	c.mu.Lock()
	c.lastErr = err
	c.mu.Unlock()
}

// Stop 幂等停止当前消费循环并等待正在执行的 handler 返回。
// Stop 不关闭 backend，成功停止后允许再次 Start；ctx 只限制当前等待时间。
func (c *Consumer) Stop(ctx context.Context) error {
	c.mu.Lock()
	if !c.running {
		err := c.lastErr
		c.mu.Unlock()
		return err
	}
	cancel := c.cancel
	done := c.done
	c.mu.Unlock()

	cancel()
	if err := waitFor(ctx, done); err != nil {
		return err
	}
	c.mu.Lock()
	err := c.lastErr
	c.mu.Unlock()
	return err
}

// Receive 在未运行 Start 消费循环时同步拉取一条消息。
// 返回 Message 携带当前 Consumer 私有确认凭据，可传给 Acknowledge/NegativeAcknowledge。
func (c *Consumer) Receive(ctx context.Context) (Message, error) {
	c.mu.Lock()
	if c.closed {
		c.mu.Unlock()
		return Message{}, ErrClosed
	}
	if c.running {
		c.mu.Unlock()
		return Message{}, ErrAlreadyStarted
	}
	c.mu.Unlock()

	backendMessage, err := c.backend.Receive(ctx)
	if err != nil {
		return Message{}, NormalizeError(err, ErrorBackend, "receive message", "")
	}
	return attachReceipt(backendMessage.Message, backendMessage.ReceiptID), nil
}

// Acknowledge 确认由当前 Consumer 接收的 message。
// ctx 控制确认请求取消和超时；其他来源的 Message 返回 ErrInvalidMessage。
func (c *Consumer) Acknowledge(ctx context.Context, message Message) error {
	receiptID, err := receiptOf(message)
	if err != nil {
		return err
	}
	return NormalizeError(c.backend.Acknowledge(ctx, receiptID), ErrorBackend, "acknowledge message", "")
}

// NegativeAcknowledge 否认由当前 Consumer 接收的 message。
// 具体重投时机由后端决定，Kafka 与 Pulsar 的底层语义可能不同。
func (c *Consumer) NegativeAcknowledge(ctx context.Context, message Message) error {
	receiptID, err := receiptOf(message)
	if err != nil {
		return err
	}
	return NormalizeError(c.backend.NegativeAcknowledge(ctx, receiptID), ErrorBackend, "negative acknowledge message", "")
}

// Unsubscribe 在消费循环停止时取消当前订阅。
// 运行中调用返回 ErrAlreadyStarted，关闭后调用返回 ErrClosed。
func (c *Consumer) Unsubscribe(ctx context.Context) error {
	c.mu.Lock()
	running := c.running
	closed := c.closed
	c.mu.Unlock()
	if closed {
		return ErrClosed
	}
	if running {
		return ErrAlreadyStarted
	}
	return NormalizeError(c.backend.Unsubscribe(ctx), ErrorBackend, "unsubscribe consumer", "")
}

// Resubscribe 在消费循环停止时重新建立相同订阅。
// 具体游标位置和重分配行为由后端及 broker 配置决定。
func (c *Consumer) Resubscribe(ctx context.Context) error {
	c.mu.Lock()
	running := c.running
	closed := c.closed
	c.mu.Unlock()
	if closed {
		return ErrClosed
	}
	if running {
		return ErrAlreadyStarted
	}
	return NormalizeError(c.backend.Resubscribe(ctx), ErrorBackend, "resubscribe consumer", "")
}

// Close 幂等停止消费循环、等待当前 handler，并关闭 backend。
// 并发 Close 共享同一 closeDone 和最终错误；关闭完成后不允许再次 Start。
func (c *Consumer) Close(ctx context.Context) error {
	c.mu.Lock()
	if c.closing {
		done := c.closeDone
		c.mu.Unlock()
		if err := waitFor(ctx, done); err != nil {
			return err
		}
		c.mu.Lock()
		err := c.closeErr
		c.mu.Unlock()
		return err
	}
	c.closing = true
	c.closed = true
	c.closeDone = make(chan struct{})
	cancel := c.cancel
	done := c.done
	running := c.running
	c.mu.Unlock()

	var closeErr error
	if running {
		cancel()
		closeErr = waitFor(ctx, done)
	}
	c.mu.Lock()
	lastErr := c.lastErr
	c.mu.Unlock()
	closeErr = errors.Join(closeErr, lastErr)
	closeErr = errors.Join(closeErr, NormalizeError(c.backend.Close(ctx), ErrorBackend, "close consumer backend", ""))

	c.mu.Lock()
	c.closeErr = closeErr
	close(c.closeDone)
	c.mu.Unlock()
	return closeErr
}
