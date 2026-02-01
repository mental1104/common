package labkit

import (
	"os"
	"path/filepath"
	"time"
)

// RunInfo holds minimal metadata for a lab run.
type RunInfo struct {
	ID        string
	StartedAt time.Time
}

// DefaultRunInfo returns a basic run identifier and timestamp.
func DefaultRunInfo() RunInfo {
	now := time.Now().UTC()
	return RunInfo{
		ID:        now.Format("20060102T150405Z"),
		StartedAt: now,
	}
}

// RunDir joins a base directory and run id into a final path.
func RunDir(base string, info RunInfo) string {
	return filepath.Join(base, info.ID)
}

// EnsureDir creates the directory if it does not exist.
func EnsureDir(path string) error {
	return os.MkdirAll(path, 0o755)
}
