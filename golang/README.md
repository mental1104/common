# Go 工具库

模块路径：`github.com/mental1104/common/golang`。

## 维护规则

新增导出函数、类型、方法、包级工具、命令或可复用实验入口时，必须在同一次变更中更新此 README，写明类别、包路径、用途、最小用法示例和备注。如果 API 已导出但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 集合与字符串包含判断
- 并发、限流与缓存协调
- CLI 与实验

## 用法索引

| 类别 | 名称 | 类型 | 导入 / 路径 | 用途 |
|---|---|---|---|---|
| 集合与字符串包含判断 | `Contains` | 函数 | `github.com/mental1104/common/golang` | 对字符串、rune、切片、数组、map 键以及受支持容器指针进行便捷包含判断。 |
| 集合与字符串包含判断 | `InSlice` | 泛型函数 | `github.com/mental1104/common/golang` | 以类型安全方式检查切片中是否存在指定值。 |
| 集合与字符串包含判断 | `InMapKey` | 泛型函数 | `github.com/mental1104/common/golang` | 以类型安全方式检查 map 中是否存在指定键。 |
| 集合与字符串包含判断 | `InMapValue` | 泛型函数 | `github.com/mental1104/common/golang` | 以类型安全方式检查 map 值中是否存在指定值。 |
| 集合与字符串包含判断 | `InString` | 函数 | `github.com/mental1104/common/golang` | 检查字符串是否包含子串。 |
| 集合与字符串包含判断 | `InRune` | 函数 | `github.com/mental1104/common/golang` | 检查字符串是否包含指定 rune。 |
| 并发与限流 | `TokenBucket`, `ErrNilContext` | 结构体 / 错误值 | `github.com/mental1104/common/golang` | 单进程内阻塞获取令牌，并支持 Context 取消。 |
| 并发与缓存协调 | `SingleFlightGroup`, `SingleFlightResult`, `ErrSingleFlightNilContext` | 泛型结构体 / 错误值 | `github.com/mental1104/common/golang` | 合并同一进程内同 Key 的并发调用，等待者可取消等待。 |
| Redis 与缓存协调 | `RedisLock`, `NewRedisLock`, `ErrRedisLockNilContext` | 结构体 / 构造函数 / 错误值 | `github.com/mental1104/common/golang` | 使用 `SET NX PX` 与 owner-check Lua 脚本实现非可重入 Redis 分布式锁。 |
| Redis 与缓存协调 | `RedisSingleFlight`, `NewRedisSingleFlight`, `RedisSingleFlightOptions`, `CacheLookup`, `CacheHit`, `CacheMiss`, `ErrRebuildTimeout` | 泛型结构体 / 函数 / 错误值 | `github.com/mental1104/common/golang` | 组合本地 singleflight、Redis 缓存、分布式锁、轮询等待与可选旧值兜底。 |
| CLI 与实验 | `labctl`, `labs/*` | 命令 | `./golang/cmd/labctl`, `./golang/labs/*` | 可运行的实验 / 演示入口。 |

## 详情

### `Contains`

- **类别：** 集合与字符串包含判断
- **类型：** 函数
- **定义位置：** `golang/mental1104/contains.go`
- **导入：** `github.com/mental1104/common/golang`
- **用途：** 用统一调用形式处理字符串、rune、切片、数组、map 键和受支持指针的包含判断。

**基础用法：**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.Contains("golang", "go"))
	fmt.Println(mental1104.Contains([]int{3, 5, 8}, 5))
	fmt.Println(mental1104.Contains(map[string]int{"a": 1}, "a"))
}
```

**示例输出：**

```text
true
true
true
```

**备注：**

- `Contains` 对 map 使用键存在语义。
- 热路径或需要类型安全时，优先使用下面的泛型辅助函数。

### `InSlice`

- **类别：** 集合与字符串包含判断
- **类型：** 泛型函数
- **定义位置：** `golang/mental1104/contains.go`
- **导入：** `github.com/mental1104/common/golang`
- **用途：** 检查可比较值是否存在于切片中。

**基础用法：**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.InSlice([]string{"a", "b"}, "b"))
}
```

**示例输出：**

```text
true
```

### `InMapKey`

- **类别：** 集合与字符串包含判断
- **类型：** 泛型函数
- **定义位置：** `golang/mental1104/contains.go`
- **导入：** `github.com/mental1104/common/golang`
- **用途：** 检查 map 中是否存在指定键。

**基础用法：**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	values := map[string]int{"a": 1}
	fmt.Println(mental1104.InMapKey(values, "a"))
}
```

**示例输出：**

```text
true
```

### `InMapValue`

- **类别：** 集合与字符串包含判断
- **类型：** 泛型函数
- **定义位置：** `golang/mental1104/contains.go`
- **导入：** `github.com/mental1104/common/golang`
- **用途：** 检查可比较值是否存在于 map 的值集合中。

**基础用法：**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	values := map[string]int{"a": 1}
	fmt.Println(mental1104.InMapValue(values, 1))
}
```

**示例输出：**

```text
true
```

### `InString`

- **类别：** 集合与字符串包含判断
- **类型：** 函数
- **定义位置：** `golang/mental1104/contains.go`
- **导入：** `github.com/mental1104/common/golang`
- **用途：** 检查字符串是否包含子串。

**基础用法：**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.InString("golang", "go"))
}
```

**示例输出：**

```text
true
```

### `InRune`

- **类别：** 集合与字符串包含判断
- **类型：** 函数
- **定义位置：** `golang/mental1104/contains.go`
- **导入：** `github.com/mental1104/common/golang`
- **用途：** 检查字符串是否包含指定 rune。

**基础用法：**

```go
package main

import (
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	fmt.Println(mental1104.InRune("golang", 'g'))
}
```

**示例输出：**

```text
true
```

### `TokenBucket`

- **类别：** 并发与限流
- **类型：** 结构体、构造函数、方法、错误值
- **定义位置：** `golang/mental1104/token_bucket.go`
- **导入：** `mental1104 "github.com/mental1104/common/golang"`
- **用途：** 在单个 Go 进程内按长期速率和突发容量阻塞获取执行资格；等待过程支持 `context.Context` 取消。

**基础用法：**

```go
package main

import (
	"context"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	bucket, err := mental1104.NewTokenBucket(20, 3)
	if err != nil {
		panic(err)
	}

	if err := bucket.Acquire(context.Background()); err != nil {
		panic(err)
	}
	bucket.Release()
}
```

**备注：**

- 创建时为满桶；`Acquire` 每次消费一个令牌，令牌通过单调时间按需补充，不创建后台 goroutine。
- `Release` 是空操作，完成任务后不会归还速率额度。
- `nil` Context 返回 `ErrNilContext`；取消返回 `context.Canceled` 或 `context.DeadlineExceeded`，且不消费令牌。
- 状态仅在单进程内有效；每个等待者拥有独立 Timer，不保证严格公平，也不提供 `TryAcquire`、批量获取或指标。

### `SingleFlightGroup`

- **类别：** 并发与缓存协调
- **类型：** 泛型结构体、方法、结果结构体、错误值
- **定义位置：** `golang/mental1104/singleflight.go`
- **导入：** `mental1104 "github.com/mental1104/common/golang"`
- **用途：** 按 Key 合并同一进程内的并发调用；首个调用者执行 loader，等待者复用同一值或同一错误。

**基础用法：**

```go
package main

import (
	"context"
	"fmt"

	mental1104 "github.com/mental1104/common/golang"
)

func main() {
	var group mental1104.SingleFlightGroup[string, string]
	result, err := group.Do(context.Background(), "product:123", func(context.Context) (string, error) {
		return `{"id":123}`, nil
	})
	if err != nil {
		panic(err)
	}
	fmt.Println(result.Value, result.Shared)
}
```

**备注：**

- `Shared=false` 表示当前调用执行了 loader；同一 in-flight 调用的等待者得到 `Shared=true`。
- 等待者取消 Context 只停止自己的等待，不会删除或破坏正在执行的共享调用。
- leader 的 loader 使用 leader 调用方传入的 Context；loader 返回错误或 panic 后，in-flight 状态会清理，后续调用可以重试。
- 该能力只在当前 Go 进程内生效，不能协调其他 Pod。

### `RedisLock`

- **类别：** Redis 与缓存协调
- **类型：** 结构体、构造函数、方法、错误值
- **定义位置：** `golang/mental1104/redis_lock.go`
- **导入：** `mental1104 "github.com/mental1104/common/golang"`
- **用途：** 使用随机 owner token、`SET key token NX PX ttl` 和 Lua 校验删除实现非可重入 Redis 分布式锁。

**基础用法：**

```go
lock, err := mental1104.NewRedisLock(client, "singleflight:lock:product:123", 3*time.Second)
if err != nil {
	return err
}
locked, err := lock.TryLock(ctx)
if err != nil {
	return err
}
if locked {
	defer lock.Unlock(context.Background())
	// 重建共享缓存。
}
```

**备注：**

- `TryLock` 只尝试一次，不在库内自旋；调用方决定等待策略。
- `Unlock` 仅在 Redis 中的 token 仍属于当前锁实例时删除 key。
- 锁没有自动续期，也不是可重入锁；任务耗时超过 TTL 时，其他实例可能重新获得锁。

### `RedisSingleFlight`

- **类别：** Redis 与缓存协调
- **类型：** 泛型结构体、构造函数、配置结构体、回调类型、结果结构体、错误值
- **定义位置：** `golang/mental1104/redis_singleflight.go`
- **导入：** `mental1104 "github.com/mental1104/common/golang"`
- **用途：** 在 Redis 缓存 miss 时，以本地 singleflight 合并当前实例请求，再用 Redis 锁选择一个集群级缓存重建者；未持锁实例轮询缓存并复用结果。

**基础用法：**

```go
package main

import (
	"context"
	"errors"
	"time"

	mental1104 "github.com/mental1104/common/golang"
	redis "github.com/redis/go-redis/v9"
)

func build(client redis.UniversalClient) (*mental1104.RedisSingleFlight[string], error) {
	options := mental1104.DefaultRedisSingleFlightOptions()
	return mental1104.NewRedisSingleFlight[string](
		client,
		func(ctx context.Context, key string) (mental1104.CacheLookup[string], error) {
			value, err := client.Get(ctx, key).Result()
			if errors.Is(err, redis.Nil) {
				return mental1104.CacheMiss[string](), nil
			}
			if err != nil {
				return mental1104.CacheLookup[string]{}, err
			}
			return mental1104.CacheHit(value), nil
		},
		func(ctx context.Context, key, value string, ttl time.Duration) error {
			return client.Set(ctx, key, value, ttl).Err()
		},
		nil, // 可选 stale getter。
		options,
	)
}
```

调用：

```go
result, err := singleflight.GetOrLoad(ctx, "product:123", func(ctx context.Context) (string, error) {
	return queryProductJSON(ctx, 123)
})
if err != nil {
	return err
}
useProductJSON(result.Value)
```

**行为与限制：**

- 顺序为：Redis 首读 → 本地 singleflight → Redis 二次检查 → 尝试锁 → 持锁后再次检查 → loader → 写缓存；未持锁者以 `PollMin`～`PollMax` jitter 轮询。
- `CacheLookup.Found` 明确区分缓存 miss 与缓存中的零值 / 空字符串。
- Redis 读写和锁错误会原样返回，不会静默伪装成 miss。
- 等待超过 `WaitTimeout` 时，若提供 stale getter 且命中，则返回 `Stale=true`；否则返回 `ErrRebuildTimeout`。
- `LockTTL`、`CacheTTL`、等待和轮询参数必须有效；默认锁 TTL 3 秒、缓存 TTL 10 分钟、等待 500ms、轮询 20～50ms。
- 这是集群级回源抑制，不是 exactly-once。网络分区、进程暂停或 loader 超过锁 TTL 时仍可能重复回源；支付、扣款等业务必须使用业务唯一键、数据库约束和幂等记录。

### 实验命令

- **类别：** CLI 与实验
- **类型：** 命令包
- **定义位置：** `golang/cmd/labctl`, `golang/labs/*`
- **导入 / 路径：** 在 `golang/` 下通过 `go run ./cmd/labctl` 或具体实验目录运行
- **用途：** 运行本地调度器、网络、内存和 GC 实验演示。

**基础用法：**

```bash
cd golang
go run ./cmd/labctl
go run ./labs/gc/minimal
```

**示例输出：**

```text
labctl demo run: docs/labs/_verify/<YYYYMMDDTHHMMSSZ>
gc minimal demo: <YYYYMMDDTHHMMSSZ>
```

**备注：**

- 待复核：实验是可运行演示，不是稳定的可复用库 API。
