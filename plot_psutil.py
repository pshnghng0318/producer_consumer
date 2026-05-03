#!/usr/bin/env python3
"""
繪製 psutil 監控數據
"""

import matplotlib.pyplot as plt
import numpy as np
import sys

def parse_psutil_log(filepath):
    """解析 psutil 日誌"""
    times = []
    cpu = []
    read_rates = []
    write_rates = []
    
    with open(filepath) as f:
        for line in f:
            if line.startswith('#'):
                continue
            parts = line.strip().split()
            if len(parts) >= 5:
                try:
                    times.append(float(parts[0]))
                    cpu.append(float(parts[1]))
                    read_rates.append(float(parts[2]))
                    write_rates.append(float(parts[3]))
                except:
                    continue
    
    return (np.array(times), np.array(cpu), 
            np.array(read_rates), np.array(write_rates))

def plot_monitoring_data(logfile, output='psutil_monitor.png'):
    """繪製監控數據"""
    times, cpu, read_rates, write_rates = parse_psutil_log(logfile)
    
    if len(times) == 0:
        print(f"錯誤: {logfile} 中沒有有效數據")
        return
    
    print(f"數據點數: {len(times)}")
    print(f"時間範圍: {times[0]:.2f} - {times[-1]:.2f} 秒")
    print(f"採樣間隔: {np.mean(np.diff(times))*1000:.1f} ms (平均)")
    print(f"讀取速率: {read_rates.mean():.2f} MB/s (平均), {read_rates.max():.2f} MB/s (峰值)")
    print(f"CPU: {cpu.mean():.1f}% (平均), {cpu.max():.1f}% (峰值)")
    
    # 創建圖表
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8))
    
    # 上圖: Disk I/O Rate
    ax1.plot(times, read_rates, label='Read (MB/s)', color='steelblue', linewidth=1.5, alpha=0.8)
    ax1.plot(times, write_rates, label='Write (MB/s)', color='coral', linewidth=1.5, alpha=0.8)
    ax1.set_xlabel('Time (s)', fontsize=11)
    ax1.set_ylabel('I/O Rate (MB/s)', fontsize=11)
    ax1.set_title(f'Disk I/O Rate - {len(times)} samples', fontsize=12, fontweight='bold')
    ax1.legend(loc='upper right', fontsize=10)
    ax1.grid(True, alpha=0.3, linestyle='--')
    
    # 下圖: CPU Usage
    ax2.plot(times, cpu, color='orange', linewidth=1.5, alpha=0.8)
    ax2.set_xlabel('Time (s)', fontsize=11)
    ax2.set_ylabel('CPU %', fontsize=11)
    ax2.set_title('CPU Usage', fontsize=12, fontweight='bold')
    ax2.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig(output, dpi=150, bbox_inches='tight')
    print(f"圖表已保存: {output}")
    plt.show()

def compare_versions(files, labels, output='psutil_comparison.png'):
    """比較多個版本"""
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8))
    
    colors = ['steelblue', 'coral', 'green', 'purple']
    
    for i, (file, label) in enumerate(zip(files, labels)):
        try:
            times, cpu, read_rates, write_rates = parse_psutil_log(file)
            color = colors[i % len(colors)]
            
            ax1.plot(times, read_rates, label=f'{label} - Read', 
                    color=color, linewidth=1.5, alpha=0.7)
            ax2.plot(times, cpu, label=label, 
                    color=color, linewidth=1.5, alpha=0.7)
            
            print(f"{label}: {len(times)} samples, "
                  f"讀取 {read_rates.mean():.2f} MB/s (avg), "
                  f"CPU {cpu.mean():.1f}% (avg)")
        except Exception as e:
            print(f"警告: 無法讀取 {file}: {e}")
    
    ax1.set_xlabel('Time (s)', fontsize=11)
    ax1.set_ylabel('Read Rate (MB/s)', fontsize=11)
    ax1.set_title('Disk I/O Rate Comparison', fontsize=12, fontweight='bold')
    ax1.legend(loc='upper right', fontsize=9)
    ax1.grid(True, alpha=0.3, linestyle='--')
    
    ax2.set_xlabel('Time (s)', fontsize=11)
    ax2.set_ylabel('CPU %', fontsize=11)
    ax2.set_title('CPU Usage Comparison', fontsize=12, fontweight='bold')
    ax2.legend(loc='upper right', fontsize=9)
    ax2.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig(output, dpi=150, bbox_inches='tight')
    print(f"比較圖表已保存: {output}")
    plt.show()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("用法:")
        print("  python3 plot_psutil.py <logfile>")
        print("  python3 plot_psutil.py compare file1 label1 file2 label2 ...")
        sys.exit(1)
    
    if sys.argv[1] == 'compare':
        files = sys.argv[2::2]
        labels = sys.argv[3::2]
        if len(files) != len(labels):
            print("錯誤: 文件和標籤數量不匹配")
            sys.exit(1)
        compare_versions(files, labels)
    else:
        logfile = sys.argv[1]
        output = sys.argv[2] if len(sys.argv) > 2 else 'psutil_monitor.png'
        plot_monitoring_data(logfile, output)
