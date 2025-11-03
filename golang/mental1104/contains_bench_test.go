package mental1104

import "testing"

func BenchmarkInSlice_Generic(b *testing.B) {
    xs := make([]int, 1000)
    for i := range xs { xs[i] = i }
    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        _ = InSlice(xs, 777)
    }
}
func BenchmarkContains_Reflect_Slice(b *testing.B) {
    xs := make([]int, 1000)
    for i := range xs { xs[i] = i }
    b.ResetTimer()
    for i := 0; i < b.N; i++ {
        _ = Contains(xs, 777)
    }
}
