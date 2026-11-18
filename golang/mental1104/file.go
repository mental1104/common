package mental1104

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
)

// MoveStatus 表示已经发布目标后的状态，发布前失败由独立 error 返回。
type MoveStatus uint8

const (
	// MoveCompleted 表示源已清理且必要的目录同步成功。
	MoveCompleted MoveStatus = iota
	// MovePublishedSourceRemains 表示目标已发布，源树可能只完成部分清理。
	MovePublishedSourceRemains
	// MoveDurabilityUncertain 表示命名成功但目录持久化无法确认。
	MoveDurabilityUncertain
)

// MoveResult 的 SourceRemoved 记录源根是否已删除；Err 仅在部分成功时非 nil。
type MoveResult struct {
	Status        MoveStatus
	SourceRemoved bool
	Err           error
}

// moveEntry 保存不跟随链接的源快照；清单后序用于保守清理。
type moveEntry struct {
	path string
	info os.FileInfo
}

// moveIO 将真实 I/O 阶段集中为本次调用的依赖；测试替换不会改变全局状态。
type moveIO struct {
	rename  func(string, string, bool) error
	syncDir func(string) error
	remove  func(string) error
	copy    func(*os.File, *os.File, int64) error
}

// syncMoveDir 同步 path 的目录项；返回真实同步错误，不推测持久化成功。
func syncMoveDir(path string) error {
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()
	return f.Sync()
}

// verifyMoveEntry 比较 path 与原始 info；identityOnly 用于全树复核后的源清理，忽略自身清理导致的时间变化。
// 返回冲突错误时不删除当前项；检测无法代替调用方的排他访问。
func verifyMoveEntry(path string, info os.FileInfo, identityOnly bool) error {
	after, err := os.Lstat(path)
	if err != nil {
		return err
	}
	if !os.SameFile(info, after) || info.Mode() != after.Mode() ||
		(!identityOnly && (info.Size() != after.Size() || !info.ModTime().Equal(after.ModTime()) || moveChangeTime(info) != moveChangeTime(after))) {
		return fmt.Errorf("source changed during move: %s", path)
	}
	return nil
}

// copyMoveBytes 从 input 分块复制到 output，核对 expected 字节数并逐块读回。
// 两个文件的生命周期由调用方持有；失败返回错误，绝不清理源。
func copyMoveBytes(input, output *os.File, expected int64) error {
	buffer := make([]byte, 65536)
	var count int64
	// 显式限制每次读写的大小，避免 io.CopyBuffer 被 File 的零拷贝快路径绕过。
	for {
		n, readErr := input.Read(buffer)
		if n > 0 {
			written, err := output.Write(buffer[:n])
			if err != nil {
				return err
			}
			if written != n {
				return io.ErrShortWrite
			}
			count += int64(n)
		}
		if readErr == io.EOF {
			break
		}
		if readErr != nil {
			return readErr
		}
		if n == 0 {
			return io.ErrNoProgress
		}
	}
	if count != expected {
		return errors.New("copy size verification failed")
	}
	if _, err := input.Seek(0, io.SeekStart); err != nil {
		return err
	}
	if _, err := output.Seek(0, io.SeekStart); err != nil {
		return err
	}
	other := make([]byte, len(buffer))
	for {
		n, aerr := io.ReadFull(input, buffer)
		m, berr := io.ReadFull(output, other)
		if aerr != nil && aerr != io.EOF && aerr != io.ErrUnexpectedEOF {
			return aerr
		}
		if berr != nil && berr != io.EOF && berr != io.ErrUnexpectedEOF {
			return berr
		}
		if n != m || !bytes.Equal(buffer[:n], other[:m]) {
			return errors.New("copy readback verification failed")
		}
		if n == 0 {
			return nil
		}
	}
}

// copyMoveRegular 独占创建 target，复制 source、恢复普通权限/mtime 并同步。
// info 是打开前快照；ops 仅为当前调用的 I/O 边界，返回任何发布前错误。
func copyMoveRegular(source, target string, info os.FileInfo, ops moveIO) (err error) {
	input, err := openMoveSource(source)
	if err != nil {
		return err
	}
	defer input.Close()
	opened, err := input.Stat()
	if err != nil {
		return err
	}
	if !os.SameFile(info, opened) || !opened.Mode().IsRegular() {
		return errors.New("source replaced before copy")
	}
	output, err := os.OpenFile(target, os.O_CREATE|os.O_EXCL|os.O_RDWR, 0600)
	if err != nil {
		return err
	}
	defer func() {
		// close 错误也发生在发布之前，不能吞掉延迟写入失败。
		if closeErr := output.Close(); err == nil {
			err = closeErr
		}
	}()
	if err = ops.copy(input, output, info.Size()); err != nil {
		return err
	}
	if err = output.Chmod(info.Mode().Perm()); err != nil {
		return err
	}
	if err = os.Chtimes(target, info.ModTime(), info.ModTime()); err != nil {
		return err
	}
	return output.Sync()
}

// copyMoveTree 将 source 复制到私有 target，不跟随符号链接。
// entries 追加源快照，ops 为当前 I/O 边界；返回首个发布前错误。
func copyMoveTree(source, target string, entries *[]moveEntry, ops moveIO) error {
	info, err := os.Lstat(source)
	if err != nil {
		return err
	}
	*entries = append(*entries, moveEntry{source, info})
	switch {
	case info.Mode()&os.ModeSymlink != 0:
		text, err := os.Readlink(source)
		if err != nil {
			return err
		}
		if err = os.Symlink(text, target); err != nil {
			return err
		}
		if err = setMoveLinkTime(target, info); err != nil {
			return err
		}
	case info.IsDir():
		if err = os.Mkdir(target, 0700); err != nil {
			return err
		}
		children, err := os.ReadDir(source)
		if err != nil {
			return err
		}
		for _, child := range children {
			if err = copyMoveTree(filepath.Join(source, child.Name()), filepath.Join(target, child.Name()), entries, ops); err != nil {
				return err
			}
		}
		if err = os.Chmod(target, info.Mode().Perm()); err != nil {
			return err
		}
		if err = os.Chtimes(target, info.ModTime(), info.ModTime()); err != nil {
			return err
		}
		if err = ops.syncDir(target); err != nil {
			return err
		}
	case info.Mode().IsRegular():
		if err = copyMoveRegular(source, target, info, ops); err != nil {
			return err
		}
	default:
		return errors.New("unsupported source type")
	}
	return verifyMoveEntry(source, info, false)
}

// verifyMoveTree 复核 entries 中所有源项，返回首个可观察冲突。
func verifyMoveTree(entries []moveEntry) error {
	for _, entry := range entries {
		if err := verifyMoveEntry(entry.path, entry.info, false); err != nil {
			return err
		}
	}
	return nil
}

// discardMoveTree 尽力回收本次私有 path；恢复目录清理权限，不跟随链接，不修改源和目标。
func discardMoveTree(path string) {
	info, err := os.Lstat(path)
	if err != nil {
		return
	}
	if info.IsDir() {
		if os.Chmod(path, 0700) != nil {
			return
		}
		items, err := os.ReadDir(path)
		if err != nil {
			return
		}
		for _, item := range items {
			discardMoveTree(filepath.Join(path, item.Name()))
		}
	}
	_ = os.Remove(path)
}

// cleanMoveSource 后序删除 entries 并逐层同步，ops 为本次 I/O 边界。
// 返回发布后的完整状态；删除失败可能留下部分源树，未知新增项不会递归删除。
func cleanMoveSource(entries []moveEntry, ops moveIO) MoveResult {
	if err := verifyMoveTree(entries); err != nil {
		return MoveResult{MovePublishedSourceRemains, false, err}
	}
	for i := len(entries) - 1; i >= 0; i-- {
		entry := entries[i]
		// 全树已复核；删除硬链接兄弟会改变同 inode 的 ctime，此处只比较身份。
		if err := verifyMoveEntry(entry.path, entry.info, true); err != nil {
			return MoveResult{MovePublishedSourceRemains, false, err}
		}
		if err := ops.remove(entry.path); err != nil {
			return MoveResult{MovePublishedSourceRemains, false, err}
		}
		if err := ops.syncDir(filepath.Dir(entry.path)); err != nil {
			return MoveResult{MoveDurabilityUncertain, i == 0, err}
		}
	}
	return MoveResult{Status: MoveCompleted, SourceRemoved: true}
}

// moveFile 执行 source 到 target 的阶段协议；overwrite 控制原子覆盖，ops 只属于本次调用。
// 发布前返回非 nil error；发布后只通过 MoveResult 报告部分成功。
func moveFile(source, target string, overwrite bool, ops moveIO) (MoveResult, error) {
	if source == "" || target == "" {
		return MoveResult{}, errors.New("invalid move path")
	}
	src, err := filepath.Abs(source)
	if err != nil {
		return MoveResult{}, err
	}
	dst, err := filepath.Abs(target)
	if err != nil {
		return MoveResult{}, err
	}
	info, err := os.Lstat(src)
	if err != nil {
		return MoveResult{}, err
	}
	if !info.IsDir() && !info.Mode().IsRegular() && info.Mode()&os.ModeSymlink == 0 {
		return MoveResult{}, errors.New("unsupported source type")
	}
	other, err := os.Lstat(dst)
	if err == nil && os.SameFile(info, other) {
		return MoveResult{}, errors.New("source and target are aliases")
	}
	if err != nil && !os.IsNotExist(err) {
		return MoveResult{}, err
	}
	if info.IsDir() {
		cursor, err := filepath.EvalSymlinks(filepath.Dir(dst))
		if err != nil {
			return MoveResult{}, err
		}
		for {
			ancestor, err := os.Stat(cursor)
			if err != nil {
				return MoveResult{}, err
			}
			if os.SameFile(info, ancestor) {
				return MoveResult{}, errors.New("target is inside source")
			}
			next := filepath.Dir(cursor)
			if next == cursor {
				break
			}
			cursor = next
		}
	}
	if err = ops.rename(src, dst, overwrite); err == nil {
		// 并发创建的源硬链接可能使覆盖 rename 成功但没有移除源，不能误报完成。
		_, sourceErr := os.Lstat(src)
		removed := os.IsNotExist(sourceErr)
		if err = ops.syncDir(filepath.Dir(dst)); err == nil {
			err = ops.syncDir(filepath.Dir(src))
		}
		if err != nil {
			return MoveResult{MoveDurabilityUncertain, removed, err}, nil
		}
		if !removed {
			if sourceErr == nil {
				sourceErr = errors.New("source remains after rename")
			}
			return MoveResult{MovePublishedSourceRemains, false, sourceErr}, nil
		}
		return MoveResult{Status: MoveCompleted, SourceRemoved: true}, nil
	} else if !isMoveCrossDevice(err) {
		return MoveResult{}, err
	}
	if !moveCrossSupported {
		return MoveResult{}, errors.New("cross-volume move unsupported on this platform")
	}
	stage, err := os.MkdirTemp(filepath.Dir(dst), ".move-")
	if err != nil {
		return MoveResult{}, err
	}
	// 私有容器内恢复的权限不会向其他用户开放；失败只回收本次临时产物。
	defer discardMoveTree(stage)
	payload := filepath.Join(stage, "payload")
	var entries []moveEntry
	if err = copyMoveTree(src, payload, &entries, ops); err != nil {
		return MoveResult{}, err
	}
	if err = verifyMoveTree(entries); err != nil {
		return MoveResult{}, err
	}
	if err = ops.syncDir(stage); err != nil {
		return MoveResult{}, err
	}
	if err = ops.syncDir(filepath.Dir(dst)); err != nil {
		return MoveResult{}, err
	}
	if err = ops.rename(payload, dst, overwrite); err != nil {
		return MoveResult{}, err
	}
	if err = os.Remove(stage); err == nil {
		err = ops.syncDir(filepath.Dir(dst))
	}
	if err != nil {
		return MoveResult{MoveDurabilityUncertain, false, err}, nil
	}
	return cleanMoveSource(entries, ops), nil
}

// MoveFile 移动普通文件、目录或链接；source 是已有项，target 是父目录已存在的最终路径。
// overwrite 默认为 false（省略第三参数），最多允许一个值；不会合并目录。
// 返回非 nil error 表示发布前失败；发布后检查结果 Status、SourceRemoved 和 Err，不能盲目重试。
// 调用方必须独占源树及祖先目录；变化检测不能防住所有并发写入。跨卷非端到端原子。
// 跨卷只保留普通权限、mtime、链接文本，不保证 uid/gid、ACL、xattr、稀疏及硬链接拓扑。
// Windows 仅支持同卷；目录同步不支持时返回 MoveDurabilityUncertain。
func MoveFile(source, target string, overwrite ...bool) (MoveResult, error) {
	if len(overwrite) > 1 {
		return MoveResult{}, errors.New("at most one overwrite option is allowed")
	}
	replace := len(overwrite) == 1 && overwrite[0]
	return moveFile(source, target, replace, moveIO{renameMoveNative, syncMoveDir, os.Remove, copyMoveBytes})
}
