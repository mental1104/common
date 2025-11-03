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
