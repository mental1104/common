package mq

import (
	"context"
	"errors"
	"sync"
)

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

func NewConsumer(backend ConsumerBackend) (*Consumer, error) {
	if backend == nil {
		return nil, NewError(ErrorInvalidConfig, "new consumer", "", "consumer backend must not be nil", nil)
	}
	return &Consumer{backend: backend}, nil
}

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
				return
			}
			if errors.Is(err, context.DeadlineExceeded) || errors.Is(err, ErrTimeout) {
				continue
			}
			c.setLastError(NormalizeError(err, ErrorBackend, "receive message", ""))
			return
		}

		message := attachReceipt(backendMessage.Message, backendMessage.ReceiptID)
		action, handlerErr := invokeHandler(ctx, handler, message)
		if handlerErr != nil {
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
		default:
			c.setLastError(NewError(ErrorHandler, "handle message", "", "unknown consume action", nil))
			if err := c.NegativeAcknowledge(ctx, message); err != nil {
				c.setLastError(err)
			}
			return
		}
	}
}

func invokeHandler(ctx context.Context, handler MessageHandler, message Message) (action ConsumeAction, err error) {
	defer func() {
		if recovered := recover(); recovered != nil {
			err = fmtPanic(recovered)
		}
	}()
	return handler(ctx, CloneMessage(message))
}

func fmtPanic(value any) error {
	return NewError(ErrorHandler, "message handler panic", "", "handler panicked", errors.New(toString(value)))
}

func toString(value any) string {
	if text, ok := value.(string); ok {
		return text
	}
	return "non-string panic value"
}

func (c *Consumer) setLastError(err error) {
	c.mu.Lock()
	c.lastErr = err
	c.mu.Unlock()
}

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

func (c *Consumer) Acknowledge(ctx context.Context, message Message) error {
	receiptID, err := receiptOf(message)
	if err != nil {
		return err
	}
	return NormalizeError(c.backend.Acknowledge(ctx, receiptID), ErrorBackend, "acknowledge message", "")
}

func (c *Consumer) NegativeAcknowledge(ctx context.Context, message Message) error {
	receiptID, err := receiptOf(message)
	if err != nil {
		return err
	}
	return NormalizeError(c.backend.NegativeAcknowledge(ctx, receiptID), ErrorBackend, "negative acknowledge message", "")
}

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
