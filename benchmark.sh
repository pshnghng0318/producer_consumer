#!/bin/bash
# 比較三個版本的性能和 I/O rate

echo "=================================="
echo "Performance Comparison Test"
echo "=================================="
echo ""

# 測試配置
THREADS=(4 8 16)

# 測試原始版本
echo ">>> Testing ORIGINAL version (single mutex)"
for T in "${THREADS[@]}"; do
    echo "  Threads: $T"
    /usr/bin/time -l ./readChunks $T 2>&1 | grep -E "real|maximum resident"
    echo ""
done

echo "=================================="
echo ""

# 測試改進版本
echo ">>> Testing IMPROVED version (bounded queue)"
for T in "${THREADS[@]}"; do
    echo "  I/O Threads: $T, Compute Threads: $T"
    /usr/bin/time -l ./readChunks_improved $T $T 2>&1 | grep -E "real|maximum resident"
    echo ""
done

echo "=================================="
echo ""

# 測試 lock-free 版本
echo ">>> Testing LOCK-FREE version (no locks)"
for T in "${THREADS[@]}"; do
    echo "  I/O Threads: $T, Compute Threads: $T"
    /usr/bin/time -l ./readChunks_lockfree $T $T 2>&1 | grep -E "real|maximum resident"
    echo ""
done

echo "=================================="
echo "Test Complete!"
echo "=================================="
