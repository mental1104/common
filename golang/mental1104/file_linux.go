package mental1104

import (
	"errors"
	"golang.org/x/sys/unix"
	"os"
	"syscall"
)

const moveCrossSupported = true

// renameMoveNative 原子移动 source 到 target；overwrite=false 交由内核排他裁决。
// 返回原始 errno，只有 EXDEV 可以进入复制降级。
func renameMoveNative(source, target string, overwrite bool) error {
	if overwrite {
		return unix.Rename(source, target)
	}
	return unix.Renameat2(unix.AT_FDCWD, source, unix.AT_FDCWD, target, unix.RENAME_NOREPLACE)
}

// isMoveCrossDevice 判断 err 是否明确为跨卷错误，不把其他 I/O 错误当作降级条件。
func isMoveCrossDevice(err error) bool { return errors.Is(err, unix.EXDEV) }

// openMoveSource 打开 path 的普通文件，不跟随最后一级链接；返回由调用方关闭的文件。
func openMoveSource(path string) (*os.File, error) {
	fd, err := unix.Open(path, unix.O_RDONLY|unix.O_NOFOLLOW|unix.O_NONBLOCK|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, err
	}
	return os.NewFile(uintptr(fd), path), nil
}

// moveChangeTime 从 info 提取纳秒 ctime，用于检测内容被修改后恢复 mtime 的情况。
func moveChangeTime(info os.FileInfo) int64 { return info.Sys().(*syscall.Stat_t).Ctim.Nano() }

// setMoveLinkTime 为 path 链接本身恢复 info 的 mtime，不跟随链接。
func setMoveLinkTime(path string, info os.FileInfo) error {
	t := unix.NsecToTimespec(info.ModTime().UnixNano())
	return unix.UtimesNanoAt(unix.AT_FDCWD, path, []unix.Timespec{t, t}, unix.AT_SYMLINK_NOFOLLOW)
}
