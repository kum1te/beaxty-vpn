#!/usr/bin/env bash
# BeaxtyVPN privilege configuration script for Linux TUN mode
# Grants SUID root (chmod 4755) and cap_net_admin to beaxty-core.
# This eliminates the requirement to launch the GUI as root while allowing
# the core daemon to create TUN interfaces, manage routing rules, and mark packets.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ -n "${1:-}" ]]; then
    INPUT_PATH="$1"
else
    INPUT_PATH="${ROOT_DIR}/bin/beaxty-core"
    if [[ ! -f "${INPUT_PATH}" && -f "${ROOT_DIR}/build/bin/beaxty-core" ]]; then
        INPUT_PATH="${ROOT_DIR}/build/bin/beaxty-core"
    fi
fi

if ! command -v realpath >/dev/null 2>&1; then
    echo "Error: realpath utility is required." >&2
    exit 1
fi

BIN_PATH="$(realpath -e "${INPUT_PATH}" 2>/dev/null || true)"

if [[ -z "${BIN_PATH}" || ! -f "${BIN_PATH}" ]]; then
    echo "Error: '${INPUT_PATH}' does not exist or is not a regular file." >&2
    exit 1
fi

BIN_NAME="$(basename "${BIN_PATH}")"
if [[ "${BIN_NAME}" != "beaxty-core" ]]; then
    echo "Error: Invalid target binary '${BIN_NAME}'. Expected 'beaxty-core'." >&2
    exit 1
fi

# Prohibit elevation of binaries located in critical system or temporary paths
for restricted_dir in "/etc" "/bin" "/usr/bin" "/sbin" "/usr/sbin" "/tmp" "/var/tmp" "/dev"; do
    if [[ "${BIN_PATH}" == "${restricted_dir}"* ]]; then
        echo "Error: Binary path '${BIN_PATH}' is inside restricted directory '${restricted_dir}'." >&2
        exit 1
    fi
done

echo "Setting permissions and capabilities on ${BIN_PATH}..."

if [[ $EUID -ne 0 ]]; then
    CMD="chown root:root '${BIN_PATH}' && chmod 4755 '${BIN_PATH}'"
    if command -v setcap >/dev/null 2>&1; then
        CMD="${CMD} && setcap 'cap_net_admin,cap_net_bind_service+ep' '${BIN_PATH}' || true"
    fi

    if command -v pkexec >/dev/null 2>&1; then
        echo "Elevating privileges with pkexec..."
        pkexec sh -c "${CMD}"
    elif command -v sudo >/dev/null 2>&1; then
        echo "Elevating privileges with sudo..."
        sudo sh -c "${CMD}"
    else
        echo "Error: Neither pkexec nor sudo found to elevate privileges." >&2
        exit 1
    fi
else
    chown root:root "${BIN_PATH}"
    chmod 4755 "${BIN_PATH}"
    if command -v setcap >/dev/null 2>&1; then
        setcap "cap_net_admin,cap_net_bind_service+ep" "${BIN_PATH}" 2>/dev/null || true
    fi
fi

echo "Permissions set successfully:"
ls -la "${BIN_PATH}"
