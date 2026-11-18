//go:build !linux && !darwin && !windows

package mental1104

import (
	"errors"
	"os"
)

const moveCrossSupported = false

// renameMoveNative 在缺少可靠原子接口的平台拒绝 source/target/overwrite 操作，返回错误。
func renameMoveNative(source, target string, overwrite bool) error {
	return errors.New("atomic move unsupported on this platform")
}

// isMoveCrossDevice 对 err 不启用任何复制降级。
func isMoveCrossDevice(err error) bool { return false }

// openMoveSource 为受限平台保留编译入口；path 返回调用方负责关闭的文件。
func openMoveSource(path string) (*os.File, error) { return os.Open(path) }

// moveChangeTime 返回 info 的 mtime，受限平台不承诺跨卷冲突检测。
func moveChangeTime(info os.FileInfo) int64 { return info.ModTime().UnixNano() }

// setMoveLinkTime 拒绝 path/info 的链接元信息恢复，返回不支持错误。
func setMoveLinkTime(path string, info os.FileInfo) error {
	return errors.New("link metadata unsupported")
}
