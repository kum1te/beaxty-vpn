//go:build linux || darwin

package parentcheck

import (
	"fmt"
	"os"
	"path/filepath"
	"syscall"
)

// A setuid-root core must never trust a GUI that can be replaced by the
// unprivileged user. Portable/user-writable bundles therefore cannot use the
// privileged mode; they must run through a separate helper instead.
func validatePrivilegedParent(parentPath, selfPath string) error {
	if os.Geteuid() != 0 {
		return nil
	}
	if filepath.Clean(filepath.Dir(parentPath)) != filepath.Clean(filepath.Dir(selfPath)) {
		return fmt.Errorf("privileged parent and core are installed in different directories")
	}
	for label, path := range map[string]string{"parent": parentPath, "core": selfPath} {
		info, err := os.Stat(path)
		if err != nil {
			return fmt.Errorf("cannot stat privileged %s: %w", label, err)
		}
		stat, ok := info.Sys().(*syscall.Stat_t)
		if !ok || stat.Uid != 0 {
			return fmt.Errorf("privileged %s is not owned by root", label)
		}
		if info.Mode().Perm()&0o022 != 0 {
			return fmt.Errorf("privileged %s is writable by group or other users", label)
		}
	}
	return nil
}
