#!/bin/bash
# 使用 fs_usage 進行高精度 I/O 監控（macOS 專用）
# 需要 sudo 權限

echo "============================================"
echo "  高精度 I/O 監控（使用 fs_usage）"
echo "============================================"
echo ""

PROGRAM_NAME=${1:-readChunks_improved}

echo "監控程式: $PROGRAM_NAME"
echo "注意：此腳本需要 sudo 權限"
echo ""

# 檢查 sudo
if [ "$EUID" -ne 0 ]; then 
    echo "請使用 sudo 運行此腳本："
    echo "  sudo ./monitor_io_hires.sh $PROGRAM_NAME"
    exit 1
fi

# 啟動程式並記錄 PID
./$PROGRAM_NAME 4 8 > ${PROGRAM_NAME}_output.txt 2>&1 &
PROGRAM_PID=$!

echo "程式 PID: $PROGRAM_PID"
echo "開始監控..."

# 使用 fs_usage 監控該程式的 file I/O
# -f filesystem = 只監控文件系統操作
# -w = wide output
fs_usage -f filesystem -w $PROGRAM_PID > fs_usage_${PROGRAM_NAME}.log 2>&1

echo "監控完成"
echo "結果已保存到 fs_usage_${PROGRAM_NAME}.log"
