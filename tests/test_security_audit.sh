#!/usr/bin/env bash
# GPL-3.0 License
# Copyright (C) 2026 BeaxtyVPN Authors
# Static least-privilege checks. This script deliberately never starts the app/core.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENGINE="${ROOT_DIR}/src/core/ThroneEngine.cpp"
BRIDGE="${ROOT_DIR}/src/bridge/MainWindowBridge.cpp"
PACKAGE="${ROOT_DIR}/scripts/package_linux.sh"
INSTALLER="${ROOT_DIR}/src/platform/linux/CapabilityInstaller.cpp"

fail() {
    printf '[FAIL] %s\n' "$1" >&2
    exit 1
}

grep -Fq 'cap_net_admin=ep' "${ENGINE}" || fail 'The TUN path does not assign the intended capability.'
grep -Fq 'beaxty-vpn-core-%1' "${ENGINE}" || fail 'The system core path is not content-derived.'
grep -Fq 'u:%1:--x,m::--x' "${INSTALLER}" || fail 'The installed core directory is not scoped to one user.'
grep -Fq 'hasExactUserSearchAcl' "${ENGINE}" || fail 'The per-user ACL is not verified before using the privileged core.'
grep -Fq 'hasFileCapabilities' "${ENGINE}" || fail 'Bundled cores are not checked for stale Linux file capabilities.'
grep -Fq 'assert_unprivileged_core' "${PACKAGE}" || fail 'Linux packaging does not reject privileged bundled cores.'
grep -Fq 'fchmod(outputFd, 0755)' "${INSTALLER}" || fail 'The installed core mode is not explicitly non-SUID.'
grep -Fq 'm_rpcConnected' "${ENGINE}" || fail 'RPC connection checks are not cross-thread-safe.'
grep -Fq 'm_workerPool.setMaxThreadCount(1)' "${ENGINE}" || fail 'State-changing RPC work is not serialized.'
grep -Fq 'm_workerPool.waitForDone()' "${ENGINE}" || fail 'Engine cleanup does not join background work.'
grep -Fq 'm_capabilitySetupProcess' "${ENGINE}" || fail 'Polkit setup is not tracked for shutdown cancellation.'
grep -Fq 'beaxty-vpn-privileged-helper' "${ENGINE}" || fail 'Linux TUN setup does not use the dedicated narrow helper.'
grep -Fq 'cap_net_admin=ep' "${INSTALLER}" || fail 'The privileged helper does not grant only CAP_NET_ADMIN.'
grep -Fq 'O_NOFOLLOW' "${INSTALLER}" || fail 'The privileged helper does not reject symlink swaps.'
grep -Fq 'developmentCore' "${INSTALLER}" || fail 'The privileged helper does not constrain its source to the bundled core.'
grep -Fq 'PKEXEC_UID' "${INSTALLER}" || fail 'The privileged helper does not use Polkit to identify the calling user.'
if rg -n 'getuid\(\)' "${INSTALLER}"; then
    fail 'The privileged helper must use PKEXEC_UID rather than its post-Polkit process UID.'
fi
[[ "$(grep -Fc 'proc->start(pkexecPath,' "${ENGINE}")" -eq 1 ]] || fail 'TUN setup does not use exactly one Polkit authorization call.'
grep -Fq 'beaxty-vpn-privileged-helper' "${PACKAGE}" || fail 'The Linux package omits the narrow TUN installer helper.'
grep -Fq 'SO_PEERCRED' "${ROOT_DIR}/src/core/LocalPeerCredentials.hpp" || fail 'Linux core IPC does not authenticate its local peer.'
grep -Fq 'isExpectedCorePeer' "${ENGINE}" || fail 'Linux core peer credentials are not enforced before RPC reconnect.'
if rg -n 'QThreadPool::globalInstance' "${ENGINE}"; then
    fail 'Connection lifecycle work still uses the process-wide thread pool.'
fi
if rg -n 'engine\.initialize\([^,]*\);' "${ROOT_DIR}"/tests/*.cpp; then
    fail 'A test engine may auto-discover and start the developer-installed core.'
fi
if rg -n 'm_rpcSocket' "${ENGINE}" "${ROOT_DIR}/src/core/ThroneEngine.hpp"; then
    fail 'The engine still accesses a socket whose ownership moved to the RPC I/O thread.'
fi
grep -Fq 'pkexecPath' "${ENGINE}" || fail 'The capability setup does not use Polkit.'

if rg -n 'chmod[[:space:]]+4755|cap_net_bind_service|cap_sys_admin|"sh".*"-c"|QStringLiteral\("-c"\)' \
    "${ENGINE}" "${BRIDGE}" "${INSTALLER}"; then
    fail 'Found an unsafe SUID, broader capability, or shell-based privilege path.'
fi

if rg -n 'Linux_Run_Command|Linux_HavePkexec|isSetuidSet|QProcess::execute\(QStringLiteral\("ip"\)' \
    "${BRIDGE}" "${ENGINE}"; then
    fail 'Found a legacy privilege or unverified route-mutation path.'
fi

[[ ! -e "${ROOT_DIR}/scripts/setup-cap.sh" ]] || fail 'The legacy SUID helper is still present.'
if rg -n 'setup-cap\.sh' "${PACKAGE}"; then
    fail 'The Linux package still includes the legacy SUID helper.'
fi

grep -Fq 'm_bundledCoreBinaryPath' "${ENGINE}" || fail 'Bundled and installed core paths are not separated.'
grep -Fq 'normal network connection' "${ENGINE}" || fail 'Unexpected core exit is not reported honestly.'
grep -Fq 'cabinetExternalBrowser' "${ROOT_DIR}/src/ui/qml/views/SettingsView.qml" || fail 'Cabinet preference UI is not bound to stored state.'
grep -Fq 'forceDarkMode: Theme.isDark' "${ROOT_DIR}/src/ui/qml/views/CabinetWebEngineComponent.qml" || fail 'Embedded cabinet does not follow the app theme.'
grep -Fq 'std::atomic_bool loop_enabled' "${ROOT_DIR}/3rdparty/throne/include/stats/traffic/TrafficLooper.hpp" || fail 'Traffic loop enable flag is shared without synchronization.'
grep -Fq 'std::atomic_bool stop_requested' "${ROOT_DIR}/3rdparty/throne/include/stats/traffic/TrafficLooper.hpp" || fail 'Traffic loop stop flag is shared without synchronization.'

printf 'Static security checks passed. No GUI, core daemon, or tunnel was started.\n'
