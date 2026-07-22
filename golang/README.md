# Go 工具库

模块路径：`github.com/mental1104/common/golang`。

## 维护规则

新增导出函数、类型、方法、包级工具、命令或可复用实验入口时，必须在同一次变更中更新此 README，写明类别、包路径、用途、最小用法示例和备注。如果 API 已导出但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 集合与字符串包含判断
- HTTP 调用结果分类
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
| HTTP 调用结果分类 | `ClassifyHTTPOutcome`, `WrapHTTPTransport`, `HTTPOutcome` | 函数 / 类型 | `github.com/mental1104/common/golang/mental1104` | 区分 HTTP 状态失败与网络失败，并包装现有 `http.RoundTripper` 上报结果。 |
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

### HTTP 调用结果分类

- **类别：** HTTP 调用结果分类
- **名称：** `HTTPOutcomeKind`, `HTTPOutcome`, `ClassifyHTTPOutcome`, `WrapHTTPTransport`
- **类型：** 枚举类型、结构体、函数、`http.RoundTripper` 中间件
- **定义位置：** `golang/mental1104/http_failure.go`
- **导入：** `mental1104 "github.com/mental1104/common/golang/mental1104"`
- **用途：** 在网关或其他出站 HTTP 调用中区分“已经收到失败状态码”和“尚未获得有效 HTTP 响应的网络错误”。

**基础用法：**

```go
package main

import (
	"fmt"
	"net/http"

	mental1104 "github.com/mental1104/common/golang/mental1104"
)

func main() {
	client := &http.Client{
		Transport: mental1104.WrapHTTPTransport(
			http.DefaultTransport,
			func(request *http.Request, outcome mental1104.HTTPOutcome) {
				fmt.Println(request.URL.Host, outcome.Kind.String(), outcome.StatusCode)
			},
		),
	}

	response, err := client.Get("https://example.com")
	if response != nil {
		defer response.Body.Close()
	}
	_ = err
}
```

**示例结果：**

```text
example.com success 200
```

**429/503 vs EOF 实验：**

```bash
cd golang
go run ./labs/http_failure
```

**实验输出：**

```text
scenario=429 do_error=false status_readable=true status_code=429 kind=http_status_failure
scenario=503 do_error=false status_readable=true status_code=503 kind=http_status_failure
scenario=eof do_error=true status_readable=false status_code=0 kind=network_failure
```

**备注：**

- `429` 和 `503` 已经形成合法 HTTP 响应，因此 `client.Do` 不返回网络 error，`resp.StatusCode` 可读。
- 直接断连发生在服务端写出 HTTP 状态行之前，因此 `client.Do` 返回 error，通常没有可读取的 response。
- 包装器不消费响应体、不关闭连接，也不把 `4xx/5xx` 转换成 Go error。
- 本能力不实现自动重试；是否重试仍需结合请求幂等性、请求体可重放性和业务语义决定。
- observer 可能被并发调用，并且其 panic 不会被包装器恢复。

### 实验命令

- **类别：** CLI 与实验
- **类型：** 命令包
- **定义位置：** `golang/cmd/labctl`, `golang/labs/*`
- **导入 / 路径：** 在 `golang/` 下通过 `go run ./cmd/labctl` 或具体实验目录运行
- **用途：** 运行本地调度器、网络、内存、GC 和 HTTP 失败语义实验演示。

**基础用法：**

```bash
cd golang
go run ./cmd/labctl
go run ./labs/gc/minimal
go run ./labs/http_failure
```

**示例输出：**

```text
labctl demo run: docs/labs/_verify/<YYYYMMDDTHHMMSSZ>
gc minimal demo: <YYYYMMDDTHHMMSSZ>
scenario=429 do_error=false status_readable=true status_code=429 kind=http_status_failure
```

**备注：**

- 待复核：实验是可运行演示，不是稳定的可复用库 API。
