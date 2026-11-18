//go:build darwin && !cgo

package mental1104

import "errors"

// renameMoveExclusive 在禁用 cgo 时明确拒绝 source/target 的排他移动，不修改路径。
func renameMoveExclusive(source, target string) error {
	return errors.New("atomic no-replace rename requires cgo on macOS")
}
