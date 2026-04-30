# Make Chunks
創一個資料夾包含4x256個大小各爲1mb ，值為隨機-1 到1 ，類型float 32的檔案
檔名數字z.x.y，x y 值為0或z為0-255，

# Read Chunks
寫一個C++讀取它們

## make an array to save data
在讀取前需要能用一個thread預先創立一個長度為256，
值為size_t的spec_mean陣列，
分為2 partition 各128個值，

## read by threads
然後再用n個threads去讀取檔案，

## mean value available and progress monitor
讀取時同步安排一個thread檢查:
若每四個檔案z.0.0 z.0.1 z.1.0 z.1.1讀完，就取一個mean值，
n可調2-32,
同時檢查進度，

## End of program
填滿spec_mean後停止並輸出stdout

# monitor thread 可以更乾淨（進階）
現在是 polling 可以改 condition variable（事件觸發）

# 是否升級成
1. lock-free queue（更快）
2. mmap 版本（IO爆速）
3. SIMD mean（AVX2/AVX512）
4. NUMA aware 分配
5. async IO (io_uring)
