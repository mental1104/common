package mental1104

import (
	"errors"
	"golang.org/x/sys/unix"
	"os"
	"syscall"
)

const moveCrossSupported = true

// renameMoveNative 将 source 原子命名为 target；overwrite 控制内核排他标记，返回系统错误。
func renameMoveNative(source, target string, overwrite bool) error {
	if overwrite {
		return unix.Rename(source, target)
	}
	return renameMoveExclusive(source, target)
}

// isMoveCrossDevice 只允许 err 为 EXDEV 时复制降级。
func isMoveCrossDevice(err error) bool { return errors.Is(err, unix.EXDEV) }

// openMoveSource 不跟随最后一级 path 链接；返回调用方负责关闭的文件。
func openMoveSource(path string) (*os.File, error) {
	fd, err := unix.Open(path, unix.O_RDONLY|unix.O_NOFOLLOW|unix.O_NONBLOCK|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	return os.NewFile(uintptr(fd), path), nil
}

// moveChangeTime 提取 info 的纳秒 ctime，用于冲突检测。
func moveChangeTime(info os.FileInfo) int64 { return info.Sys().(*syscall.Stat_t).Ctimespec.Nano() }

// setMoveLinkTime 将 info 的 mtime 恢复到 path 链接本身，返回系统错误。
func setMoveLinkTime(path string, info os.FileInfo) error {
	t := unix.NsecToTimespec(info.ModTime().UnixNano())
	return unix.UtimesNanoAt(unix.AT_FDCWD, path, []unix.Timespec{t, t}, unix.AT_SYMLINK_NOFOLLOW)
}
