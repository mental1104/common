可以！下面给你一套**最小可落地**的步骤 + 代码：调用侧既可用**一个统一函数名 `Contains`**（像 C++ 重载一样自动“看类型”），也提供**类型安全的泛型函数**（无反射，高性能）。附带表格测试。

---

## 1) 目录与初始化

在你的仓库中新建 Go 子模块：

```bash
cd common
mkdir -p golang/mental1104
cd golang
go mod init mental1104.top/common/golang   # 用你的 vanity 域名；没有就先用 github.com/你/common/golang
go mod tidy
```

（可选）在仓库根建立 workspace，便于在根目录直接 `go test ./...`：

```bash
cd ..
go work init ./golang
go work use ./golang
```

---

## 2) 代码：统一 `Contains` + 泛型快捷函数

**golang/mental1104/contains.go**

```go
package mental1104

import (
	"reflect"
	"strings"
)

// Contains：统一入口（像 C++ 重载一样的调用体验）。
// 支持：
//   - string: needle 为 string 或 rune
//   - slice/array: 线性查找（使用 reflect.DeepEqual，适配任意元素类型）
//   - map: 以“键是否存在”为语义（值判断见 ContainsMapValue）
//
// 说明：这是便利函数，内部用到了反射；高频热路径建议使用下面的泛型版本。
func Contains(haystack any, needle any) bool {
	// 先处理最常用的 string
	switch s := haystack.(type) {
	case string:
		switch n := needle.(type) {
		case string:
			return strings.Contains(s, n)
		case rune:
			return strings.ContainsRune(s, n)
		default:
			return false
		}
	}

	v := reflect.ValueOf(haystack)

	// 解引用指针（兼容 *[]T、*[N]T、*map[K]V）
	if v.IsValid() && v.Kind() == reflect.Ptr {
		if v.IsNil() {
			return false
		}
		v = v.Elem()
	}

	switch v.Kind() {
	case reflect.Slice, reflect.Array:
		for i := 0; i < v.Len(); i++ {
			if reflect.DeepEqual(v.Index(i).Interface(), needle) {
				return true
			}
		}
		return false

	case reflect.Map:
		// 语义：判断“键是否存在”
		keyType := v.Type().Key()
		k := reflect.ValueOf(needle)
		if !k.IsValid() {
			return false
		}
		// 类型不匹配时尝试可转换
		if k.Type() != keyType {
			if k.Type().ConvertibleTo(keyType) {
				k = k.Convert(keyType)
			} else {
				return false
			}
		}
		return v.MapIndex(k).IsValid()

	default:
		return false
	}
}

// --------- 泛型（类型安全 / 高性能）---------

// InSlice：元素是否在切片/数组里（数组请用 arr[:] 传入）
func InSlice[T comparable, S ~[]T](s S, v T) bool {
	for _, x := range s {
		if x == v {
			return true
		}
	}
	return false
}

// InMapKey：键是否在 map 中（等价 Python: k in dict）
func InMapKey[K comparable, V any, M ~map[K]V](m M, k K) bool {
	_, ok := m[k]
	return ok
}

// InMapValue：值是否在 map 的 values 里（等价 Python: v in dict.values()）
func InMapValue[K comparable, V comparable, M ~map[K]V](m M, v V) bool {
	for _, x := range m {
		if x == v {
			return true
		}
	}
	return false
}

// InString：子串是否存在
func InString(s, sub string) bool { return strings.Contains(s, sub) }

// InRune：rune 是否存在
func InRune(s string, r rune) bool { return strings.ContainsRune(s, r) }
```

> 语义约定：**`Contains(map, needle)` 默认按“键存在性”**。若要按值查找，用 `InMapValue`。

---

## 3) 单元测试（表格用例覆盖 slice/array/map/string/指针等）

**golang/mental1104/contains_test.go**

```go
package mental1104

import "testing"

func TestContains_String(t *testing.T) {
	if !Contains("golang", "go") { t.Fatal("sub string should exist") }
	if !Contains("你好golang", '你') { t.Fatal("rune should exist") }
	if Contains("abc", 123) { t.Fatal("mismatched needle type should be false") }
}

func TestContains_Slice(t *testing.T) {
	if !Contains([]int{3,5,8}, 5) { t.Fatal("5 should be in slice") }
	if Contains([]int{3,5,8}, 7) { t.Fatal("7 should not be in slice") }
	type S struct{ A int }
	if !Contains([]S{{1},{2}}, S{2}) { t.Fatal("struct value should be matched by DeepEqual") }
}

func TestContains_Array(t *testing.T) {
	arr := [3]string{"a","b","c"}
	if !Contains(arr, "b") { t.Fatal("b should be in array") }
	if Contains(&arr, "x") { t.Fatal("x should not be in array (pointer support)") }
}

func TestContains_MapKey(t *testing.T) {
	m := map[string]int{"a":1, "b":2}
	if !Contains(m, "a") { t.Fatal("key a should exist") }
	if Contains(m, "x") { t.Fatal("key x should not exist") }
	// 可转换键类型（例如 int32 -> int）
	m2 := map[int]string{1:"x"}
	var k int32 = 1
	if !Contains(m2, k) { t.Fatal("convertible key should work") }
}

func TestInSlice_Generic(t *testing.T) {
	if !InSlice([]int{1,2,3}, 2) { t.Fatal("InSlice basic") }
	arr := [3]int{1,2,3}
	if !InSlice(arr[:], 3) { t.Fatal("InSlice with array via slicing") }
}

func TestInMap_Generic(t *testing.T) {
	m := map[string]int{"a":1, "b":2}
	if !InMapKey(m, "a") { t.Fatal("InMapKey") }
	if !InMapValue(m, 2) { t.Fatal("InMapValue") }
}

func TestInString_Generic(t *testing.T) {
	if !InString("abcdef", "bc") { t.Fatal("InString") }
	if !InRune("你好", '你') { t.Fatal("InRune") }
}
```

---

## 4) 运行测试

```bash
cd common/golang
go test ./...
```

---

## 5) 调用侧示例

```go
import "mental1104.top/common/golang/mental1104"

// 方便（像“重载”）：自动按类型判断
mental1104.Contains([]int{1,2,3}, 2)               // true
mental1104.Contains([...]string{"a","b"}, "b")     // true
mental1104.Contains(map[string]int{"a":1}, "a")    // true（按键）
mental1104.Contains("golang", "go")                // true
mental1104.Contains("你好", '你')                    // true

// 高性能（泛型，无反射）
mental1104.InSlice([]int{1,2,3}, 2)
mental1104.InMapKey(map[string]int{"a":1}, "a")
mental1104.InMapValue(map[string]int{"a":1}, 1)
mental1104.InString("golang", "go")
```

---

### 设计取舍说明

* Go 不支持函数重载；**统一的 `Contains(any, any)` 用反射实现“重载体验”**，让调用最简；同时**提供泛型版本**覆盖高频场景，避免反射开销。
* **array** 无法用泛型统一（缺少 const generics），所以：

  * `Contains` 直接支持数组（反射路径），
  * 泛型路径用 `arr[:]` 转成 slice。

需要的话，我可以再在 `README.md` 里放性能对比（反射 vs 泛型）。


## 性能测试

不是的。**不需要**专门叫成 `_bench_test.go`。

* 只要文件名以 **`_test.go`** 结尾，里头有符合签名的 **`BenchmarkXxx(*testing.B)`** 函数，`go test -bench` 就会发现并运行它们。
* 也就是说，基准可以和普通单测放在同一个 `*_test.go` 里，或分开到 `*_bench_test.go` ——后者只是**团队约定的可读性命名**，不是硬性要求。

最小示例：

```go
// file: contains_test.go  (名字随意，只要 *_test.go)
package mental1104

import "testing"

func BenchmarkInSlice(b *testing.B) {
    xs := make([]int, 1000)
    for i := range xs { xs[i] = i }
    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        _ = InSlice(xs, 777)
    }
}
```

常用命令小抄：

* 跑所有基准：`go test -bench . ./...`
* 只跑基准、不跑单测：`go test -run=^$ -bench . ./...`
* 看内存分配：`go test -run=^$ -bench . -benchmem`
* 调整时长/轮次：`-benchtime=3s` 或 `-benchtime=100000x`
* 多核：`-cpu=1,2,4`
* 重复多次取中位数：`-count=5`
