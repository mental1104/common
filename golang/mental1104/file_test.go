package mental1104

import (
	"bytes"
	"errors"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"syscall"
	"testing"
	"time"
)

// TestMoveFileMinimalUsage 为 t 展示准备资源、公开移动、读取结果和自动清理的最短完整流程。
func TestMoveFileMinimalUsage(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("Windows reports restricted durability")
	}
	dir := t.TempDir()
	source, target := filepath.Join(dir, "source"), filepath.Join(dir, "target")
	// 写入并关闭源；TempDir 在测试完成后回收全部文件资源。
	if err := os.WriteFile(source, []byte("hello"), 0600); err != nil {
		t.Fatal(err)
	}
	result, err := MoveFile(source, target)
	if err != nil {
		t.Fatal(err)
	}
	// 同时核对结果状态、目标内容与源目录项，避免仅验证“不报错”。
	data, err := os.ReadFile(target)
	_, sourceErr := os.Lstat(source)
	if err != nil || string(data) != "hello" || result.Status != MoveCompleted || !result.SourceRemoved || result.Err != nil || !os.IsNotExist(sourceErr) {
		t.Fatalf("unexpected move result: %+v %v", result, err)
	}
}

// moveTestDestination 为 t 创建目标目录；cross=true 时要求 /dev/shm 与 source 位于不同设备。
// 返回实际目录并注册清理；没有第二文件系统时明确跳过。
func moveTestDestination(t *testing.T, source string, cross bool) string {
	t.Helper()
	if !cross {
		return source
	}
	if runtime.GOOS != "linux" {
		t.Skip("second filesystem unavailable")
	}
	a, err := os.Stat(source)
	if err != nil {
		t.Fatal(err)
	}
	b, err := os.Stat("/dev/shm")
	if err != nil || moveDevice(a) == moveDevice(b) {
		t.Skip("second filesystem unavailable")
	}
	dir, err := os.MkdirTemp("/dev/shm", "common-move-")
	if err != nil {
		t.Fatal(err)
	}
	// 清理只作用于本场景创建的私有目录。
	t.Cleanup(func() { os.RemoveAll(dir) })
	return dir
}

// TestMoveFileFilesAndTrees 用 t 验证真实同卷/跨卷的空/大文件、权限/mtime、目录与链接。
func TestMoveFileFilesAndTrees(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("POSIX behavior suite")
	}
	for _, cross := range []bool{false, true} {
		// 每个子场景独占源和目标目录，避免跨场景残留。
		t.Run(map[bool]string{false: "same", true: "real-cross"}[cross], func(t *testing.T) {
			dir := t.TempDir()
			dest := moveTestDestination(t, dir, cross)
			for _, size := range []int{0, 17, 2*1024*1024 + 137} {
				source, target := filepath.Join(dir, "source"), filepath.Join(dest, "target")
				data := bytes.Repeat([]byte{0, 255, 17}, size/3+1)[:size]
				if err := os.WriteFile(source, data, 0640); err != nil {
					t.Fatal(err)
				}
				stamp := time.Unix(1600000000, 123456789)
				if err := os.Chtimes(source, stamp, stamp); err != nil {
					t.Fatal(err)
				}
				result, err := MoveFile(source, target)
				if err != nil || result.Status != MoveCompleted {
					t.Fatalf("move failed: %+v %v", result, err)
				}
				actual, err := os.ReadFile(target)
				info, statErr := os.Stat(target)
				if err != nil || statErr != nil || !bytes.Equal(data, actual) || info.Mode().Perm() != 0640 || !info.ModTime().Equal(stamp) {
					t.Fatal("bytes or metadata changed")
				}
				os.Remove(target)
			}
			source, target := filepath.Join(dir, "tree"), filepath.Join(dest, "tree-target")
			if err := os.MkdirAll(filepath.Join(source, "empty"), 0700); err != nil {
				t.Fatal(err)
			}
			if err := os.WriteFile(filepath.Join(source, "data"), []byte("tree"), 0600); err != nil {
				t.Fatal(err)
			}
			if err := os.Link(filepath.Join(source, "data"), filepath.Join(source, "alias")); err != nil {
				t.Fatal(err)
			}
			if err := os.Symlink("missing", filepath.Join(source, "link")); err != nil {
				t.Fatal(err)
			}
			result, err := MoveFile(source, target)
			if err != nil || result.Status != MoveCompleted {
				t.Fatalf("tree move failed: %+v %v", result, err)
			}
			text, err := os.Readlink(filepath.Join(target, "link"))
			data, _ := os.ReadFile(filepath.Join(target, "data"))
			if err != nil || text != "missing" || string(data) != "tree" {
				t.Fatal("tree/link changed")
			}
			if err := os.Symlink("missing-root", filepath.Join(dir, "root-link")); err != nil {
				t.Fatal(err)
			}
			if result, err := MoveFile(filepath.Join(dir, "root-link"), filepath.Join(dest, "moved-link")); err != nil || result.Status != MoveCompleted {
				t.Fatal("root link move failed")
			}
		})
	}
}

// TestMoveFileConflicts 用 t 验证两种卷模式的默认拒绝、显式覆盖和非空目录冲突。
func TestMoveFileConflicts(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("POSIX behavior suite")
	}
	for _, cross := range []bool{false, true} {
		dir := t.TempDir()
		dest := moveTestDestination(t, dir, cross)
		source, target := filepath.Join(dir, "source"), filepath.Join(dest, "target")
		os.WriteFile(source, []byte("new"), 0600)
		os.Symlink("missing", target)
		if _, err := MoveFile(source, target); err == nil {
			t.Fatal("existing target accepted")
		}
		if result, err := MoveFile(source, target, true); err != nil || result.Status != MoveCompleted {
			t.Fatalf("overwrite failed: %v", err)
		}
		if data, _ := os.ReadFile(target); string(data) != "new" {
			t.Fatal("wrong replacement")
		}
		os.Mkdir(source, 0700)
		os.Remove(target)
		os.Mkdir(target, 0700)
		os.WriteFile(filepath.Join(target, "old"), []byte("old"), 0600)
		os.WriteFile(filepath.Join(source, "new"), []byte("new"), 0600)
		os.Chmod(source, 0500)
		if _, err := MoveFile(source, target, true); err == nil {
			t.Fatal("nonempty directory replaced")
		}
		os.Chmod(source, 0700)
		items, _ := os.ReadDir(dest)
		for _, item := range items {
			if strings.HasPrefix(item.Name(), ".move-") {
				t.Fatal("read-only staging tree leaked")
			}
		}
		os.Remove(filepath.Join(target, "old"))
		if result, err := MoveFile(source, target, true); err != nil || result.Status != MoveCompleted {
			t.Fatal("empty directory overwrite failed")
		}
	}
}

// TestMoveFileAliases 用 t 验证同路径、硬链接和源目录后代不会被误删。
func TestMoveFileAliases(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("POSIX behavior suite")
	}
	dir := t.TempDir()
	source, alias := filepath.Join(dir, "source"), filepath.Join(dir, "alias")
	os.WriteFile(source, []byte("safe"), 0600)
	if err := os.Link(source, alias); err != nil {
		t.Fatal(err)
	}
	for _, target := range []string{source, alias, filepath.Join(dir, "missing", "target")} {
		if _, err := MoveFile(source, target, true); err == nil {
			t.Fatal("invalid move accepted")
		}
	}
	tree := filepath.Join(dir, "tree")
	os.Mkdir(tree, 0700)
	if _, err := MoveFile(tree, filepath.Join(tree, "child")); err == nil {
		t.Fatal("descendant accepted")
	}
}

// TestMoveFileFaults 用 t 注入发布前/后故障，并通过真实目标内容核验结果。
func TestMoveFileFaults(t *testing.T) {
	if runtime.GOOS != "linux" {
		t.Skip("Linux fault injection")
	}
	for _, phase := range []string{"copy", "interrupt", "race", "change", "delete", "publish-sync", "source-sync", "rename-denied"} {
		// phase 指定故障边界；每个场景使用独立依赖，无全局 hook。
		t.Run(phase, func(t *testing.T) {
			dir := t.TempDir()
			source, target := filepath.Join(dir, "source"), filepath.Join(dir, "target")
			os.WriteFile(source, []byte("original"), 0600)
			published := false
			ops := moveIO{renameMoveNative, syncMoveDir, os.Remove, copyMoveBytes}
			// src/dst 为当前命名操作，overwrite 原样交给内核；只有首次操作模拟 EXDEV。
			ops.rename = func(src, dst string, overwrite bool) error {
				if src == source {
					if phase == "rename-denied" {
						return os.ErrPermission
					}
					return moveCrossError()
				}
				if phase == "race" {
					os.WriteFile(dst, []byte("competitor"), 0600)
				}
				err := renameMoveNative(src, dst, overwrite)
				published = err == nil
				return err
			}
			// input/output 属于本次复制，expected 是原始长度；故障前保留部分临时写入。
			ops.copy = func(input, output *os.File, expected int64) error {
				if phase == "copy" || phase == "interrupt" {
					output.Write([]byte("partial"))
					if phase == "copy" {
						return syscall.ENOSPC
					}
					return io.ErrUnexpectedEOF
				}
				if err := copyMoveBytes(input, output, expected); err != nil {
					return err
				}
				if phase == "change" {
					return os.WriteFile(source, []byte("modified-source"), 0600)
				}
				return nil
			}
			// path 为即将删除的已核验源项，模拟权限拒绝。
			ops.remove = func(path string) error {
				if phase == "delete" {
					return os.ErrPermission
				}
				return os.Remove(path)
			}
			// path 为真实目录；发布后或源删除后分别模拟同步失败。
			ops.syncDir = func(path string) error {
				_, err := os.Lstat(source)
				if published && (phase == "publish-sync" || (phase == "source-sync" && os.IsNotExist(err))) {
					return errors.New("injected sync failure")
				}
				return syncMoveDir(path)
			}
			result, err := moveFile(source, target, false, ops)
			if phase == "copy" || phase == "interrupt" || phase == "race" || phase == "change" || phase == "rename-denied" {
				if err == nil {
					t.Fatal("prepublication failure accepted")
				}
				if _, err := os.Lstat(source); err != nil {
					t.Fatal("source lost")
				}
				data, targetErr := os.ReadFile(target)
				if phase == "race" {
					if string(data) != "competitor" {
						t.Fatal("competitor overwritten")
					}
				} else if !os.IsNotExist(targetErr) {
					t.Fatal("failed copy published")
				}
			} else {
				want := MoveDurabilityUncertain
				if phase == "delete" {
					want = MovePublishedSourceRemains
				}
				if err != nil || result.Status != want || result.SourceRemoved != (phase == "source-sync") || result.Err == nil {
					t.Fatalf("wrong partial result: %+v %v", result, err)
				}
				if data, _ := os.ReadFile(target); string(data) != "original" {
					t.Fatal("published data lost")
				}
			}
			left, _ := filepath.Glob(filepath.Join(dir, ".move-*"))
			if len(left) != 0 {
				t.Fatal("temporary tree leaked")
			}
		})
	}
}

// TestMoveFileNativeAftermath 用 t 验证覆盖硬链接竞争和同卷 rename 后的同步失败。
func TestMoveFileNativeAftermath(t *testing.T) {
	if runtime.GOOS != "linux" {
		t.Skip("Linux fault injection")
	}
	for _, phase := range []string{"alias-race", "sync"} {
		dir := t.TempDir()
		source, target := filepath.Join(dir, "source"), filepath.Join(dir, "target")
		if err := os.WriteFile(source, []byte("safe"), 0600); err != nil {
			t.Fatal(err)
		}
		ops := moveIO{renameMoveNative, syncMoveDir, os.Remove, copyMoveBytes}
		// src/dst 在原生命名前模拟硬链接竞争，overwrite 原样交给内核。
		ops.rename = func(src, dst string, overwrite bool) error {
			if phase == "alias-race" {
				if err := os.Link(src, dst); err != nil {
					return err
				}
			}
			return renameMoveNative(src, dst, overwrite)
		}
		// path 是已成功 rename 的父目录，sync 场景返回实际 EIO 错误值。
		if phase == "sync" {
			ops.syncDir = func(path string) error { return syscall.EIO }
		}
		result, err := moveFile(source, target, true, ops)
		want := MoveDurabilityUncertain
		if phase == "alias-race" {
			want = MovePublishedSourceRemains
		}
		if err != nil || result.Status != want || result.SourceRemoved != (phase == "sync") {
			t.Fatalf("wrong native aftermath: %+v %v", result, err)
		}
		if data, _ := os.ReadFile(target); string(data) != "safe" {
			t.Fatal("target data lost")
		}
	}
}

// TestMoveFilePartialDirectoryCleanup 用 t 验证真实删除一个源子项后失败，目标完整、源树部分残留。
func TestMoveFilePartialDirectoryCleanup(t *testing.T) {
	if runtime.GOOS != "linux" {
		t.Skip("Linux fault injection")
	}
	dir := t.TempDir()
	source, target := filepath.Join(dir, "tree"), filepath.Join(dir, "target")
	os.Mkdir(source, 0700)
	os.WriteFile(filepath.Join(source, "a"), []byte("a"), 0600)
	os.WriteFile(filepath.Join(source, "b"), []byte("b"), 0600)
	ops := moveIO{renameMoveNative, syncMoveDir, os.Remove, copyMoveBytes}
	count := 0
	// src 为源根时模拟 EXDEV，dst/overwrite 仍通过原生排他发布。
	ops.rename = func(src, dst string, overwrite bool) error {
		if src == source {
			return moveCrossError()
		}
		return renameMoveNative(src, dst, overwrite)
	}
	// path 的第一项真实删除，第二项返回权限错误；不会递归删除剩余源树。
	ops.remove = func(path string) error {
		count++
		if count == 2 {
			return os.ErrPermission
		}
		return os.Remove(path)
	}
	result, err := moveFile(source, target, false, ops)
	if err != nil || result.Status != MovePublishedSourceRemains || result.SourceRemoved {
		t.Fatalf("wrong partial result: %+v %v", result, err)
	}
	a, _ := os.ReadFile(filepath.Join(target, "a"))
	b, _ := os.ReadFile(filepath.Join(target, "b"))
	left, _ := os.ReadDir(source)
	if string(a) != "a" || string(b) != "b" || len(left) != 1 {
		t.Fatal("wrong partial tree cleanup")
	}
}
