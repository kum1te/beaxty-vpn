//go:build !debug

package parentcheck

import (
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

func CheckParentProcess() {
	parentPath, err := getParentExePath(ParentPID)
	if err != nil {
		log.Fatalf("parent check: cannot read parent executable: %v", err)
	}
	parentPath = resolveFinalPath(parentPath)

	selfPath, err := os.Executable()
	if err != nil {
		log.Fatalf("parent check: cannot read own executable: %v", err)
	}
	selfPath = resolveFinalPath(selfPath)

	parentBase := filepath.Base(parentPath)

	if runtime.GOOS == "windows" {
		if !strings.EqualFold(parentBase, "Throne.exe") && !strings.EqualFold(parentBase, "beaxty-vpn.exe") && !strings.HasPrefix(strings.ToLower(parentBase), "test_") {
			log.Fatalf("parent check failed: unexpected parent %q, selfPath is %q", parentPath, selfPath)
		}
		return
	}

	if parentBase != "Throne" && parentBase != "beaxty-vpn" && !strings.HasPrefix(parentBase, "test_") {
		log.Fatalf("parent check failed: unexpected parent %q, selfPath is %q", parentPath, selfPath)
	}
}
