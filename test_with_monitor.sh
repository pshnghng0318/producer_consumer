#!/bin/bash
# 完整測試腳本：同時運行程式和監控 I/O

echo "============================================"
echo "  Producer-Consumer I/O 測試"
echo "============================================"
echo ""

# 選擇要測試的版本
VERSION=${1:-improved}  # 默認測試 improved 版本
IO_THREADS=${2:-4}
COMPUTE_THREADS=${3:-8}

if [ "$VERSION" = "original" ]; then
    PROGRAM="./readChunks"
    ARGS="$IO_THREADS"
    echo "測試版本: Original (單一 mutex)"
    echo "Worker threads: $IO_THREADS"
elif [ "$VERSION" = "lockfree" ]; then
    PROGRAM="./readChunks_lockfree"
    ARGS="$IO_THREADS $COMPUTE_THREADS"
    echo "測試版本: Lock-Free"
    echo "I/O threads: $IO_THREADS, Compute threads: $COMPUTE_THREADS"
else
    PROGRAM="./readChunks_improved"
    ARGS="$IO_THREADS $COMPUTE_THREADS"
    echo "測試版本: Improved (Bounded Queue)"
    echo "I/O threads: $IO_THREADS, Compute threads: $COMPUTE_THREADS"
fi

echo ""
echo "輸出檔案:"
echo "  - iostat_${VERSION}.log (I/O 統計)"
echo "  - ${VERSION}_output.txt (程式輸出)"
echo ""

# 確認程式存在
if [ ! -f "$PROGRAM" ]; then
    echo "錯誤: $PROGRAM 不存在"
    echo "請先編譯: g++ -std=c++17 -O3 -pthread ${PROGRAM#./}.cpp -o $PROGRAM"
    exit 1
fi

echo "開始測試..."
echo ""

# 背景執行 iostat 監控（macOS 最小間隔 1 秒）
iostat -w 1 -c 300 > iostat_${VERSION}.log 2>&1 &
IOSTAT_PID=$!

# 等待一下讓 iostat 開始
sleep 0.5

# 執行程式
echo ">>> 運行 $PROGRAM $ARGS"
time $PROGRAM $ARGS > ${VERSION}_output.txt 2>&1

# 等待一下讓 iostat 捕獲最後的 I/O
sleep 1

# 停止 iostat
kill $IOSTAT_PID 2>/dev/null

echo ""
echo "測試完成！"
echo ""
echo "查看結果:"
echo "  cat ${VERSION}_output.txt              # 程式輸出"
echo "  tail iostat_${VERSION}.log            # I/O 統計"
echo "  python3 procplot_readChunks.py        # 視覺化（需要調整檔名）"
echo ""
