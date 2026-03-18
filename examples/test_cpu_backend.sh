#!/bin/bash
#
# test_cpu_backend.sh — Validate CPU backend against known workload
#
# Runs cpu_memstress in background to generate sustained CPU DRAM traffic,
# then measures with memory_traffic_example cpu backend.
# Compares profiler measurement against memstress's algorithmic bandwidth.
#
# Prerequisites:
#   - Device connected via adb (root, perf_event_paranoid=-1)
#   - Binaries already pushed to /data/local/tmp/
#     (memory_traffic_example, cpu_memstress, libQualcommProfiler*.so)
#
# Usage: ./test_cpu_backend.sh [stress_duration_sec] [array_size_mb]

set -e

STRESS_DUR=${1:-10}
ARRAY_MB=${2:-256}
DEVICE_DIR=/data/local/tmp
PROFILER_DUR=3  # memory_traffic_example runs for 3 seconds

echo "=== CPU Backend Validation ==="
echo "Stress: ${STRESS_DUR}s, array: ${ARRAY_MB} MB, profiler: ${PROFILER_DUR}s"
echo ""

# 1. Start cpu_memstress in background on device
echo "[1/3] Starting cpu_memstress on device..."
adb shell "nohup ${DEVICE_DIR}/cpu_memstress ${STRESS_DUR} ${ARRAY_MB} \
    > /dev/null 2>${DEVICE_DIR}/cpu_memstress.log &"
sleep 2  # let memstress warm up

# 2. Run profiler
echo "[2/3] Running memory_traffic_example cpu..."
adb shell "LD_LIBRARY_PATH=${DEVICE_DIR} ${DEVICE_DIR}/memory_traffic_example cpu"

# 3. Wait for memstress to finish, show its output
echo ""
echo "[3/3] Waiting for cpu_memstress to finish..."
REMAINING=$((STRESS_DUR - PROFILER_DUR - 2))
if [ "$REMAINING" -gt 0 ]; then
    sleep "$REMAINING"
fi
sleep 1

echo ""
echo "=== cpu_memstress results ==="
adb shell "cat ${DEVICE_DIR}/cpu_memstress.log"
echo ""
echo "=== Comparison ==="
echo "If profiler Total Memory Traffic / ${PROFILER_DUR}s ≈ 1.9x bw_alg, the CPU backend is working correctly."
echo "(The 1.9x factor is HW prefetcher amplification on LPDDR5X.)"
