//go:build darwin && cgo

package mental1104

/*
#include <stdio.h>
#include <stdlib.h>
*/
import "C"
import "unsafe"

// renameMoveExclusive 将 source 原子排他命名为 target；C 字符串只在调用期间拥有。
// 返回 Darwin errno；没有以目标存在性预检查模拟排他能力。
func renameMoveExclusive(source, target string) error {
	src, dst := C.CString(source), C.CString(target)
	defer C.free(unsafe.Pointer(src))
	defer C.free(unsafe.Pointer(dst))
	rc, err := C.renamex_np(src, dst, C.RENAME_EXCL)
	if rc == 0 {
		return nil
	}
	return err
}
