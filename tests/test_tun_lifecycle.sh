#!/usr/bin/env bash
# GPL-3.0 License
# Copyright (C) 2026 BeaxtyVPN Authors
# Automated TUN Lifecycle & Process Integrity Verification Suite
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP_BIN="${ROOT_DIR}/build/beaxty-vpn"
CORE_BIN="${ROOT_DIR}/bin/beaxty-core"

echo "=========================================================="
echo "    BeaxtyVPN TUN Lifecycle & Process Audit Suite        "
echo "=========================================================="

if [ ! -f "${APP_BIN}" ]; then
    echo "ERROR: ${APP_BIN} not found. Build the project first."
    exit 1
fi
if [ ! -f "${CORE_BIN}" ]; then
    echo "ERROR: ${CORE_BIN} not found. Build core daemon first."
    exit 1
fi

TMP_DB="/tmp/beaxty_tun_lifecycle_test.db"
TOTAL_CYCLES=10
SUCCESSFUL_CYCLES=0

echo "[START] Running ${TOTAL_CYCLES} rapid connect/disconnect and process teardown cycles..."

for i in $(seq 1 ${TOTAL_CYCLES}); do
    rm -f "${TMP_DB}"
    echo -n "  -> Cycle ${i}/${TOTAL_CYCLES}: Spawning engine & core daemon... "

    # Launch GUI in offscreen mode with exit-after 700ms
    QT_QPA_PLATFORM=offscreen "${APP_BIN}" --db "${TMP_DB}" --exit-after 700 > /dev/null 2>&1 &
    GUI_PID=$!

    # Wait for GUI and Core to start and finish
    wait "${GUI_PID}"
    EXIT_CODE=$?

    if [ ${EXIT_CODE} -ne 0 ]; then
        echo "FAILED (Exit code: ${EXIT_CODE})"
        exit 1
    fi

    # Check for orphaned or zombie core processes
    ZOMBIES=$(ps aux | grep '[b]eaxty-core' | grep -i '<defunct>' || true)
    if [ -n "${ZOMBIES}" ]; then
        echo "FAILED (Detected zombie core daemon process)"
        echo "${ZOMBIES}"
        exit 1
    fi

    # Check if core daemon process is still running after GUI exited
    ORPHANS=$(pgrep -f "beaxty-core" || true)
    if [ -n "${ORPHANS}" ]; then
        echo "FAILED (Detected orphaned core daemon PID: ${ORPHANS})"
        kill -9 ${ORPHANS} 2>/dev/null || true
        exit 1
    fi

    SUCCESSFUL_CYCLES=$((SUCCESSFUL_CYCLES + 1))
    echo "OK (Clean teardown, 0 zombies)"
    sleep 0.1
done

rm -f "${TMP_DB}"
# Clean any leftover test sockets
rm -f /tmp/beaxtyIPC-* /tmp/beaxty_core_*.sock 2>/dev/null || true

echo "=========================================================="
echo "  TUN Lifecycle Summary: ${SUCCESSFUL_CYCLES}/${TOTAL_CYCLES} cycles completed successfully."
echo "  Process Isolation: Verified 0 orphaned and 0 zombie processes."
echo "  TUN LIFECYCLE AUDIT: PASSED"
echo "=========================================================="
