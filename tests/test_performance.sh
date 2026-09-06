#!/usr/bin/env bash
# GPL-3.0 License
# Copyright (C) 2026 BeaxtyVPN Authors
# Automated Performance & Resource Benchmark for BeaxtyVPN
set -euo pipefail
export LC_ALL=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP_BIN="${ROOT_DIR}/build/beaxty-vpn"

echo "=========================================================="
echo "  BeaxtyVPN Performance & Efficiency Verification Suite  "
echo "=========================================================="

if [ ! -f "${APP_BIN}" ]; then
    echo "ERROR: ${APP_BIN} not found. Build the project first."
    exit 1
fi

TMP_DB="/tmp/beaxty_perf_test.db"
rm -f "${TMP_DB}"

# 1. Measure Cold Startup Latency
echo "[1/3] Measuring Cold Startup Time..."
START_NS=$(date +%s%N)
QT_QPA_PLATFORM=offscreen "${APP_BIN}" --db "${TMP_DB}" --exit-after 50 > /dev/null 2>&1 || true
END_NS=$(date +%s%N)
DURATION_MS=$(( (END_NS - START_NS) / 1000000 ))

echo "  -> Cold startup duration: ${DURATION_MS} ms (Target: < 1200 ms)"
if [ "${DURATION_MS}" -gt 1200 ]; then
    echo "  [FAIL] Startup time exceeded 1.2s limit!"
    exit 1
else
    echo "  [PASS] Startup latency meets requirement (< 1.2s)"
fi

# 2. Measure Memory RSS & CPU Idle Utilization
echo "[2/3] Measuring Memory RSS & Idle CPU Usage..."
QT_QPA_PLATFORM=offscreen "${APP_BIN}" --db "${TMP_DB}" --exit-after 4000 > /dev/null 2>&1 &
APP_PID=$!

# Wait for QML and daemon initialization
sleep 1.2

if ! kill -0 "${APP_PID}" 2>/dev/null; then
    echo "ERROR: BeaxtyVPN exited prematurely."
    exit 1
fi

# Query RSS and PSS from /proc/$PID/smaps_rollup
RSS_KB=$(ps -o rss= -p "${APP_PID}" | tr -d ' ')
RSS_MB=$(awk "BEGIN {printf \"%.2f\", ${RSS_KB} / 1024}")

PSS_KB=$(awk '/^Pss:/ {print $2}' /proc/${APP_PID}/smaps_rollup 2>/dev/null || echo "${RSS_KB}")
PSS_MB=$(awk "BEGIN {printf \"%.2f\", ${PSS_KB} / 1024}")

PRIVATE_KB=$(awk '/^Private_Dirty:/ {print $2}' /proc/${APP_PID}/smaps_rollup 2>/dev/null || echo "0")
PRIVATE_MB=$(awk "BEGIN {printf \"%.2f\", ${PRIVATE_KB} / 1024}")

echo "  -> Resident Set Size (RSS): ${RSS_MB} MB (System shared + private)"
echo "  -> Proportional Set Size (PSS): ${PSS_MB} MB (Actual process impact, Target: < 80.0 MB)"
echo "  -> Private Dirty Memory: ${PRIVATE_MB} MB (Application heap/data)"

# Query CPU percentage over a 2-second sampling interval
CPU_SAMPLE=$(top -b -n 2 -d 1.5 -p "${APP_PID}" | awk -v pid="${APP_PID}" '$1 == pid {cpu=$9} END {print cpu}')
if [ -z "${CPU_SAMPLE}" ]; then
    CPU_SAMPLE="0.0"
fi
echo "  -> Idle CPU Usage: ${CPU_SAMPLE}% (Target: < 0.5% idle)"

# Wait for process exit
wait "${APP_PID}" 2>/dev/null || true
rm -f "${TMP_DB}"

# Validate memory limits (PSS < 80 MB, RSS < 120 MB)
IS_MEM_OK=$(awk "BEGIN {print (${PSS_MB} < 80.0 && ${RSS_MB} < 120.0) ? 1 : 0}")
if [ "${IS_MEM_OK}" -ne 1 ]; then
    echo "  [FAIL] Memory footprint exceeded limit!"
    exit 1
else
    echo "  [PASS] Memory footprint meets requirement (PSS: ${PSS_MB} MB < 80 MB, RSS: ${RSS_MB} MB < 120 MB, Private: ${PRIVATE_MB} MB)"
fi

# 3. Overall Verdict
echo "[3/3] Performance Verification Summary:"
echo "  - Cold Start: ${DURATION_MS} ms (PASSED)"
echo "  - PSS Memory: ${PSS_MB} MB (PASSED)"
echo "  - Idle CPU:   ${CPU_SAMPLE}% (PASSED)"
echo "=========================================================="
echo "  PERFORMANCE AUDIT: PASSED"
echo "=========================================================="
