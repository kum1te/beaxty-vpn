#!/usr/bin/env bash
# BeaxtyVPN privilege configuration script for Linux TUN mode
# Grants SUID root (chmod 4755) and cap_net_admin to beaxty-core.
# This eliminates the requirement to launch the GUI as root while allowing
# the core daemon to create TUN interfaces, manage routing rules, and mark packets.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN_PATH="${ROOT_DIR}/bin/beaxty-core"

if [[ ! -f "${BIN_PATH}" ]]; then
    if [[ -f "${ROOT_DIR}/build/bin/beaxty-core" ]]; then
        BIN_PATH="${ROOT_DIR}/build/bin/beaxty-core"
    else
        echo "Error: ${BIN_PATH} does not exist. Run scripts/build_core.sh first." >&2
        exit 1
    fi
fi

echo "Setting SUID root privileges on ${BIN_PATH}..."
if [[ $EUID -ne 0 ]]; then
    if command -v pkexec >/dev/null 2>&1; then
        echo "Elevating privileges with pkexec..."
        pkexec sh -c "chown root:root '${BIN_PATH}' && chmod 4755 '${BIN_PATH}'"
    elif command -v sudo >/dev/null 2>&1; then
        echo "Elevating privileges with sudo..."
        sudo chown root:root "${BIN_PATH}"
        sudo chmod 4755 "${BIN_PATH}"
    else
        echo "Error: Neither pkexec nor sudo found to elevate privileges." >&2
        exit 1
    fi
else
    chown root:root "${BIN_PATH}"
    chmod 4755 "${BIN_PATH}"
fi

# Also set capabilities if setcap is available
if command -v setcap >/dev/null 2>&1; then
    if [[ $EUID -ne 0 ]]; then
        if command -v sudo >/dev/null 2>&1; then
            sudo setcap "cap_net_admin,cap_net_bind_service+ep" "${BIN_PATH}" 2>/dev/null || true
        fi
    else
        setcap "cap_net_admin,cap_net_bind_service+ep" "${BIN_PATH}" 2>/dev/null || true
    fi
fi

echo "Permissions set successfully:"
ls -la "${BIN_PATH}"
