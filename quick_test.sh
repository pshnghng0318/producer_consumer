#!/bin/bash
# 簡化版測試：使用 iostat 1 秒間隔監控

echo "============================================"
echo "  Quick Test with I/O Monitoring"
echo "============================================"
echo ""

VERSION=${1:-improved}

case $VERSION in
    original)
        PROGRAM="./readChunks"
        ARGS="8"
        ;;
    lockfree)
        PROGRAM="./readChunks_lockfree"
        ARGS="4 8"
        ;;
    *)
        PROGRAM="./readChunks_improved"
        ARGS="4 8"
        ;;
esac

echo "測試: $PROGRAM $ARGS"
echo "監控間隔: 1 秒（macOS iostat 限制）"
echo ""

# 背景啟動 iostat
iostat -w 1 -c 300 > iostat_${VERSION}.log 2>&1 &
IOSTAT_PID=$!

sleep 1

# 運行程式
echo ">>> 開始執行..."
time $PROGRAM $ARGS

# 等待 iostat 記錄最後的數據
sleep 2
kill $IOSTAT_PID 2>/dev/null

echo ""
echo "完成！查看結果:"
echo "  tail -20 iostat_${VERSION}.log"
