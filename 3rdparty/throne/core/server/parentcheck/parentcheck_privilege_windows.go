//go:build windows

package parentcheck

func validatePrivilegedParent(_, _ string) error { return nil }
