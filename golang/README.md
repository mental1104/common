# Go 工具库

模块路径：`github.com/mental1104/common/golang`。

## 维护规则

新增导出函数、类型、方法、包级工具、命令或可复用实验入口时，必须在同一次变更中更新此 README，写明类别、包路径、用途、最小用法示例和备注。如果 API 已导出但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 集合与字符串包含判断
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
| 并发与韧性 | `CircuitBreaker`, `CircuitBreakerConfig`, `CircuitPermit`, `ErrCircuitOpen`, `Execute` | 状态 / 结构体 / 错误值 / 泛型函数 | `github.com/mental1104/common/golang/mental1104` | 基于失败率和慢调用率保护本地的下游接口调用。 |
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

### `CircuitBreaker`

- **类别：** 并发与韧性
- **类型：** 状态、配置、结构体、许可、快照、错误值和泛型函数
- **定义位置：** `golang/mental1104/circuit_breaker.go`
- **导入：** `mental1104 "github.com/mental1104/common/golang/mental1104"`
- **用途：** 在调用方进程内按下游服务与接口维护 Closed / Open / Half-Open 状态，基于精确时间滑窗统计系统失败和慢调用。

**基础用法：**

```go
config := mental1104.DefaultCircuitBreakerConfig()
breaker, err := mental1104.NewCircuitBreaker(config)
if err != nil {
    panic(err)
}
result, err := mental1104.ExecuteOrFallback(
    breaker,
    reserveStock,
    func(err error) bool { return !errors.Is(err, ErrInventoryShortage) },
    func(*mental1104.CircuitOpenError) (ReserveResult, error) {
        return cachedUnavailableResult(), nil
    },
)
```

**备注：**

- `RecordIgnored` 用于库存不足、参数错误等正常业务结果；它不进入 Closed 统计窗口，在 Half-Open 中只要不超慢阈值就视为健康探针。
- `ErrorClassifier` 返回 `true` 表示系统失败；传 `nil` 时所有非空 error 都计为失败。fallback 只在 `TryAcquire` 返回 `ErrCircuitOpen` 时执行。
- Half-Open 每轮最多发放配置数量的探针；任一失败或慢探针立即重新 Open，达到成功条件且没有在途探针后 Closed。
- 统计使用 Go `time.Time` 的单调部分；实现线程安全、无后台 goroutine，并提供 `Snapshot` 与 `WithStateChangeListener`。
- 熔断器不负责超时、限并发或重试。Open 状态应禁止重试，并与 Context 超时、Bulkhead、有限重试和 jitter 配套使用。
- 应按“下游服务 + 接口 + 调用类型”创建实例，不要做成 Redis 集中式熔断器，也不要按高基数业务 ID 建实例。

