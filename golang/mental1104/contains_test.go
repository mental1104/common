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
