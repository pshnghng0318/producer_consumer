#!/bin/bash
# 演示改進效果：比較原始版本和改進版本

echo "============================================"
echo "Producer-Consumer Model 改進效果演示"
echo "============================================"
echo ""

echo "【測試 1】原始版本 - 8 個 worker threads"
echo "問題：所有線程競爭同一個 mutex，I/O rate 不穩定"
echo "---"
echo "執行: ./readChunks 8"
echo ""
time ./readChunks 8 > /tmp/original_output.txt 2>&1
tail -5 /tmp/original_output.txt
echo ""
echo ""

echo "【測試 2】改進版本 - 4 I/O + 4 Compute threads"
echo "改進：分離 I/O 和計算，使用 bounded queue"
echo "---"
echo "執行: ./readChunks_improved 4 4"
echo ""
time ./readChunks_improved 4 4 > /tmp/improved_output.txt 2>&1
grep -E "Total time|Throughput" /tmp/improved_output.txt
echo ""
echo ""

echo "【測試 3】Lock-Free 版本 - 4 I/O + 8 Compute threads"
echo "改進：完全無鎖，純 atomic 操作"
echo "---"
echo "執行: ./readChunks_lockfree 4 8"
echo ""
time ./readChunks_lockfree 4 8 > /tmp/lockfree_output.txt 2>&1
grep -E "Total time|Throughput" /tmp/lockfree_output.txt
echo ""
echo ""

echo "============================================"
echo "結果總結"
echo "============================================"
echo ""
echo "原始版本："
grep "real" /tmp/original_output.txt 2>/dev/null || echo "（請查看完整輸出）"
echo ""
echo "改進版本："
grep "Total time" /tmp/improved_output.txt
grep "Throughput" /tmp/improved_output.txt
echo ""
echo "Lock-Free 版本："
grep "Total time" /tmp/lockfree_output.txt
grep "Throughput" /tmp/lockfree_output.txt
echo ""
echo "============================================"
echo "監控建議："
echo "  1. 使用 iostat 監控實際 I/O rate"
echo "  2. 使用 top/htop 監控 CPU 使用率"
echo "  3. 觀察改進版本的 queue size 變化"
echo "============================================"
