//go:build !linux

package mental1104

import (
	"errors"
	"os"
)

// moveDevice 在缺少测试挂载的平台返回占位值，info 不用于跨卷验收。
func moveDevice(info os.FileInfo) uint64 { return 0 }

// moveCrossError 为被跳过的 Linux 故障场景提供编译入口，返回不支持错误。
func moveCrossError() error { return errors.New("cross-volume injection unsupported") }
