package mental1104

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"sync"
	"time"

	redis "github.com/redis/go-redis/v9"
)

const redisUnlockIfOwnerScript = `
if redis.call("get", KEYS[1]) == ARGV[1] then
    return redis.call("del", KEYS[1])
end
return 0
`

var ErrRedisLockNilContext = errors.New("redis lock: context must not be nil")

type redisLockCommands interface {
	setNX(context.Context, string, string, time.Duration) (bool, error)
	evalInt(context.Context, string, []string, ...any) (int64, error)
}

type goRedisLockCommands struct {
	client redis.UniversalClient
}

func (c goRedisLockCommands) setNX(
	ctx context.Context,
	key string,
	value string,
	ttl time.Duration,
) (bool, error) {
	return c.client.SetNX(ctx, key, value, ttl).Result()
}

func (c goRedisLockCommands) evalInt(
	ctx context.Context,
	script string,
	keys []string,
	args ...any,
) (int64, error) {
	return c.client.Eval(ctx, script, keys, args...).Int64()
}

type RedisLock struct {
	mu       sync.Mutex
	commands redisLockCommands
	key      string
	token    string
	ttl      time.Duration
	locked   bool
}

func NewRedisLock(client redis.UniversalClient, key string, ttl time.Duration) (*RedisLock, error) {
	if client == nil {
		return nil, errors.New("redis lock: client must not be nil")
	}
	return newRedisLock(goRedisLockCommands{client: client}, key, ttl)
}

func newRedisLock(commands redisLockCommands, key string, ttl time.Duration) (*RedisLock, error) {
	if commands == nil {
		return nil, errors.New("redis lock: commands must not be nil")
	}
	if key == "" {
		return nil, errors.New("redis lock: key must not be empty")
	}
	if ttl <= 0 {
		return nil, errors.New("redis lock: ttl must be positive")
	}

	token, err := newRedisLockToken()
	if err != nil {
		return nil, err
	}
	return &RedisLock{commands: commands, key: key, token: token, ttl: ttl}, nil
}

func (l *RedisLock) TryLock(ctx context.Context) (bool, error) {
	if ctx == nil {
		return false, ErrRedisLockNilContext
	}

	l.mu.Lock()
	defer l.mu.Unlock()
	if l.locked {
		return false, nil
	}

	locked, err := l.commands.setNX(ctx, l.key, l.token, l.ttl)
	if err != nil {
		return false, err
	}
	l.locked = locked
	return locked, nil
}

func (l *RedisLock) Unlock(ctx context.Context) (bool, error) {
	if ctx == nil {
		return false, ErrRedisLockNilContext
	}

	l.mu.Lock()
	defer l.mu.Unlock()
	if !l.locked {
		return false, nil
	}

	deleted, err := l.commands.evalInt(
		ctx,
		redisUnlockIfOwnerScript,
		[]string{l.key},
		l.token,
	)
	if err != nil {
		return false, err
	}
	l.locked = false
	return deleted == 1, nil
}

func newRedisLockToken() (string, error) {
	var raw [16]byte
	if _, err := rand.Read(raw[:]); err != nil {
		return "", err
	}
	return hex.EncodeToString(raw[:]), nil
}
