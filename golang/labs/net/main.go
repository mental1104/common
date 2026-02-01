package main

import (
	"fmt"
	"net"

	"github.com/mental1104/common/golang/internal/labkit"
)

func main() {
	info := labkit.DefaultRunInfo()
	ip := net.ParseIP("127.0.0.1")
	fmt.Printf("net demo: %s ip=%v\n", info.ID, ip)
}
