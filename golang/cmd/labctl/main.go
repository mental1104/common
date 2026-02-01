package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/mental1104/common/golang/internal/labkit"
)

func main() {
	var out string
	flag.StringVar(&out, "out", "docs/labs/_verify", "output base directory for run artifacts")
	flag.Parse()

	info := labkit.DefaultRunInfo()
	runDir := labkit.RunDir(out, info)
	if err := labkit.EnsureDir(runDir); err != nil {
		fmt.Fprintf(os.Stderr, "labctl: failed to create %s: %v\n", runDir, err)
		os.Exit(1)
	}
	fmt.Printf("labctl demo run: %s\n", runDir)
}
