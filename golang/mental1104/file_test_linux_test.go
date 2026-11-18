//go:build linux

package mental1104

import (
	"os"
	"syscall"
	"testing"
)

// moveDevice 返回 info 的设备 ID，仅用于真实跨卷测试的挂载前提检查。
func moveDevice(info os.FileInfo) uint64 { return uint64(info.Sys().(*syscall.Stat_t).Dev) }

// moveCrossError 返回真实 EXDEV 错误值，用于可控复制分支注入。
func moveCrossError() error { return syscall.EXDEV }

// TestMoveFileSpecialTypes 用 t 核验 FIFO 不会被误当作普通字节文件读取。
func TestMoveFileSpecialTypes(t *testing.T) {
	dir := t.TempDir()
	source := dir + "/fifo"
	if err := syscall.Mkfifo(source, 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := MoveFile(source, dir+"/target"); err == nil {
		t.Fatal("FIFO accepted")
	}
}
