package main

import (
	"fmt"
	"runtime"

	"github.com/mental1104/common/golang/internal/labkit"
)

func main() {
	info := labkit.DefaultRunInfo()
	runtime.GC()
	fmt.Printf("gc minimal demo: %s\n", info.ID)
}
