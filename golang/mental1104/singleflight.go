package mental1104

import (
	"context"
	"errors"
	"sync"
)

var ErrSingleFlightNilContext = errors.New("singleflight: context must not be nil")

type SingleFlightResult[V any] struct {
	Value  V
	Shared bool
}

type singleFlightCall[V any] struct {
	done       chan struct{}
	value      V
	err        error
	panicValue any
	panicked   bool
}

type SingleFlightGroup[K comparable, V any] struct {
	mu    sync.Mutex
	calls map[K]*singleFlightCall[V]
}

func (g *SingleFlightGroup[K, V]) Do(
	ctx context.Context,
	key K,
	loader func(context.Context) (V, error),
) (SingleFlightResult[V], error) {
	if ctx == nil {
		return SingleFlightResult[V]{}, ErrSingleFlightNilContext
	}
	if loader == nil {
		return SingleFlightResult[V]{}, errors.New("singleflight: loader must not be nil")
	}

	g.mu.Lock()
	if g.calls == nil {
		g.calls = make(map[K]*singleFlightCall[V])
	}
	if call, ok := g.calls[key]; ok {
		g.mu.Unlock()
		select {
		case <-call.done:
			if call.panicked {
				panic(call.panicValue)
			}
			return SingleFlightResult[V]{Value: call.value, Shared: true}, call.err
		case <-ctx.Done():
			return SingleFlightResult[V]{}, ctx.Err()
		}
	}

	call := &singleFlightCall[V]{done: make(chan struct{})}
	g.calls[key] = call
	g.mu.Unlock()

	g.runLeader(ctx, key, call, loader)
	if call.panicked {
		panic(call.panicValue)
	}
	return SingleFlightResult[V]{Value: call.value, Shared: false}, call.err
}

func (g *SingleFlightGroup[K, V]) runLeader(
	ctx context.Context,
	key K,
	call *singleFlightCall[V],
	loader func(context.Context) (V, error),
) {
	defer func() {
		g.mu.Lock()
		if current, ok := g.calls[key]; ok && current == call {
			delete(g.calls, key)
		}
		close(call.done)
		g.mu.Unlock()
	}()
	defer func() {
		if recovered := recover(); recovered != nil {
			call.panicValue = recovered
			call.panicked = true
		}
	}()

	call.value, call.err = loader(ctx)
}
