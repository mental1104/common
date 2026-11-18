package mental1104

import (
	"errors"
	"golang.org/x/sys/windows"
	"os"
)

const moveCrossSupported = false

// renameMoveNative 将 source 原子移动到 target；overwrite 控制替换，禁止系统自动跨卷复制。
func renameMoveNative(source, target string, overwrite bool) error {
	src, err := windows.UTF16PtrFromString(source)
	if err != nil {
		return err
	}
	dst, err := windows.UTF16PtrFromString(target)
	if err != nil {
		return err
	}
	var flags uint32
	if overwrite {
		flags = windows.MOVEFILE_REPLACE_EXISTING
	}
	return windows.MoveFileEx(src, dst, flags)
}

// isMoveCrossDevice 只识别 err 中的 ERROR_NOT_SAME_DEVICE。
func isMoveCrossDevice(err error) bool { return errors.Is(err, windows.ERROR_NOT_SAME_DEVICE) }

// openMoveSource 为受限平台提供编译入口；跨卷流程在调用之前被拒绝。
// path 为源路径，返回调用方关闭的文件及错误。
func openMoveSource(path string) (*os.File, error) { return os.Open(path) }

// moveChangeTime 在受限平台仅以 info 的 mtime 作标记，不承诺跨卷检测。
func moveChangeTime(info os.FileInfo) int64 { return info.ModTime().UnixNano() }

// setMoveLinkTime 在跨卷禁用的平台明确拒绝 path/info 的链接元信息恢复。
func setMoveLinkTime(path string, info os.FileInfo) error {
	return errors.New("link metadata unsupported")
}
