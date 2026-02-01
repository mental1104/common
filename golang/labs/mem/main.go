package main

import (
	"fmt"
	"runtime"

	"github.com/mental1104/common/golang/internal/labkit"
)

func main() {
	info := labkit.DefaultRunInfo()
	buf := make([]byte, 1024)
	runtime.KeepAlive(buf)
	fmt.Printf("mem demo: %s size=%d\n", info.ID, len(buf))
}
