package mental1104

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"errors"
	"time"

	redis "github.com/redis/go-redis/v9"
)

const redisUnlockIfOwnerScript = `
if redis.call("get", KEYS[1]) == ARGV[1] then
    return redis.call("del", KEYS[1])
end
return 0
`

type RedisLock struct {
	client redis.UniversalClient
	key    string
	token  string
	ttl    time.Duration
	locked bool
}

func NewRedisLock(client redis.UniversalClient, key string, ttl time.Duration) (*RedisLock, error) {
	if client == nil {
		return nil, errors.New("redis lock: client must not be nil")
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
	return &RedisLock{client: client, key: key, token: token, ttl: ttl}, nil
}

func (l *RedisLock) TryLock(ctx context.Context) (bool, error) {
	if ctx == nil {
		return false, ErrSingleFlightNilContext
	}
	if l.locked {
		return false, nil
	}

	locked, err := l.client.SetNX(ctx, l.key, l.token, l.ttl).Result()
	if err != nil {
		return false, err
	}
	l.locked = locked
	return locked, nil
}

func (l *RedisLock) Unlock(ctx context.Context) (bool, error) {
	if ctx == nil {
		return false, ErrSingleFlightNilContext
	}
	if !l.locked {
		return false, nil
	}

	deleted, err := l.client.Eval(ctx, redisUnlockIfOwnerScript, []string{l.key}, l.token).Int64()
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
