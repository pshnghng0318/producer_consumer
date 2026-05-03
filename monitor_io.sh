#!/bin/bash
# macOS 專用的 I/O 監控腳本

echo "開始監控 disk I/O..."
echo "按 Ctrl+C 停止監控"
echo ""

# macOS iostat 限制：最小間隔為 1 秒
# -w 1 = 每秒採樣一次
# -c 120 = 採樣 120 次（2 分鐘）

iostat -w 1 -c 120 > iostat_readChunks.log

echo "監控完成，結果已保存到 iostat_readChunks.log"
