# Go 工具库

模块路径：`github.com/mental1104/common/golang`。

## 维护规则

新增导出函数、类型、方法、包级工具、命令或可复用实验入口时，必须在同一次变更中更新此 README，写明类别、包路径、用途、最小用法示例和备注。如果 API 已导出但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 集合与字符串包含判断
- CLI 与实验
- 重试与退避

## 用法索引

| 类别 | 名称 | 类型 | 导入 / 路径 | 用途 |
|---|---|---|---|---|
| 集合与字符串包含判断 | `Contains` | 函数 | `github.com/mental1104/common/golang` | 对字符串、rune、切片、数组、map 键以及受支持容器指针进行便捷包含判断。 |
| 集合与字符串包含判断 | `InSlice` | 泛型函数 | `github.com/mental1104/common/golang` | 以类型安全方式检查切片中是否存在指定值。 |
| 集合与字符串包含判断 | `InMapKey` | 泛型函数 | `github.com/mental1104/common/golang` | 以类型安全方式检查 map 中是否存在指定键。 |
| 集合与字符串包含判断 | `InMapValue` | 泛型函数 | `github.com/mental1104/common/golang` | 以类型安全方式检查 map 值中是否存在指定值。 |
| 集合与字符串包含判断 | `InString` | 函数 | `github.com/mental1104/common/golang` | 检查字符串是否包含子串。 |
| 集合与字符串包含判断 | `InRune` | 函数 | `github.com/mental1104/common/golang` | 检查字符串是否包含指定 rune。 |
| CLI 与实验 | `labctl`, `labs/*` | 命令 | `./golang/cmd/labctl`, `./golang/labs/*` | 可运行的实验 / 演示入口。 |
| 重试与退避 | `retry.Do` | 函数 | `github.com/mental1104/common/golang/mental1104/retry` | 以最大尝试次数、指数退避、抖动、错误分类和 context deadline 控制重试。 |

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

### `retry.Do`

- **类别：** 重试与退避
- **类型：** 函数与配置类型
- **定义位置：** `golang/mental1104/retry`
- **导入：** `github.com/mental1104/common/golang/mental1104/retry`
- **用途：** 对短暂网络失败等可重试错误执行指数退避，并由 context 限制总体 deadline。

**基础用法：**

```go
package main

import (
	"context"
	"time"

	"github.com/mental1104/common/golang/mental1104/retry"
)

func main() {
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()

	_ = retry.Do(ctx, retry.Options{
		MaxAttempts: 4,
		BaseDelay:   100 * time.Millisecond,
		MaxDelay:    time.Second,
		JitterRatio: 0.2,
		Retryable: func(err error) bool {
			return err != nil
		},
	}, func() error {
		return callRemoteService()
	})
}

func callRemoteService() error { return nil }
```

**备注：**

- `MaxAttempts` 包含第一次调用，因此 4 次尝试最多只会等待 3 次。
- 下一次退避放不进剩余 deadline 时直接返回 `context.DeadlineExceeded`，不会继续 sleep。
- `Sleep`、`Now` 和 `Random` 是测试注入点，普通调用通常留空。
- `Retryable` 应由业务区分临时错误和永久错误；参数错误、认证失败等通常不应重试。
