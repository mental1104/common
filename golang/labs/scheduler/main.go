package main

import (
	"fmt"
	"sync"
	"time"

	"github.com/mental1104/common/golang/internal/labkit"
)

func main() {
	info := labkit.DefaultRunInfo()
	var wg sync.WaitGroup
	for i := 0; i < 2; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			time.Sleep(10 * time.Millisecond)
			fmt.Printf("worker %d done\n", id)
		}(i)
	}
	wg.Wait()
	fmt.Printf("scheduler demo: %s\n", info.ID)
}
