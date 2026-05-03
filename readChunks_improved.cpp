#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

const int Z = 256;
const int FILES_PER_Z = 4;
const size_t N = 262144;

// 數據包：包含檔案數據和元信息
struct DataPacket {
    int z, x, y;
    std::vector<float> data;
    
    DataPacket() : z(0), x(0), y(0) {}
    DataPacket(int z_, int x_, int y_) : z(z_), x(x_), y(y_), data(N) {}
};

// 任務：只包含檔案信息
struct Task {
    int z, x, y;
};

// 結果累加器
struct Slot {
    std::atomic<int> count{0};
    std::atomic<int64_t> sum{0};
};

// === Producer-Consumer Queue with bounded buffer ===
class BoundedQueue {
private:
    std::queue<DataPacket> queue_;
    std::mutex mtx_;
    std::condition_variable cv_push_;  // 通知可以 push
    std::condition_variable cv_pop_;   // 通知可以 pop
    size_t max_size_;
    bool finished_;
    
public:
    BoundedQueue(size_t max_size) : max_size_(max_size), finished_(false) {}
    
    // Producer: 將數據放入隊列（會阻塞直到有空間）
    bool push(DataPacket&& packet) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_push_.wait(lock, [this] { 
            return queue_.size() < max_size_ || finished_; 
        });
        
        if (finished_) return false;
        
        queue_.push(std::move(packet));
        cv_pop_.notify_one();
        return true;
    }
    
    // Consumer: 從隊列取數據（會阻塞直到有數據）
    bool pop(DataPacket& packet) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_pop_.wait(lock, [this] { 
            return !queue_.empty() || finished_; 
        });
        
        if (queue_.empty() && finished_) return false;
        
        packet = std::move(queue_.front());
        queue_.pop();
        cv_push_.notify_one();
        return true;
    }
    
    // 標記生產結束
    void set_finished() {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            finished_ = true;
        }
        cv_push_.notify_all();
        cv_pop_.notify_all();
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }
};

// === Task Queue (輕量級，只分發任務) ===
class TaskQueue {
private:
    std::queue<Task> queue_;
    std::mutex mtx_;
    
public:
    void push(Task task) {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(task);
    }
    
    bool pop(Task& task) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return false;
        task = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    bool empty() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }
};

// === Global State ===
TaskQueue task_queue;
BoundedQueue data_queue(32);  // 緩衝區大小：32 個數據包（~32MB）
Slot slots[Z];
std::atomic<bool> finished[Z];
std::vector<size_t> spec_mean(256);
std::atomic<int> completed_count{0};

// === Producer: 專門讀取檔案 ===
void producer() {
    while (true) {
        Task task;
        if (!task_queue.pop(task)) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (task_queue.empty()) break;
            continue;
        }
        
        // 讀取檔案
        std::string fname = "data/" +
            std::to_string(task.z) + "." +
            std::to_string(task.x) + "." +
            std::to_string(task.y);
        
        DataPacket packet(task.z, task.x, task.y);
        std::ifstream in(fname, std::ios::binary);
        if (!in) {
            std::cerr << "Failed to open: " << fname << "\n";
            continue;
        }
        in.read(reinterpret_cast<char*>(packet.data.data()), N * sizeof(float));
        
        // 放入緩衝區（可能會阻塞）
        if (!data_queue.push(std::move(packet))) {
            break;
        }
    }
}

// === Consumer: 專門計算 ===
void consumer() {
    constexpr double scale = 1e6;
    
    while (true) {
        DataPacket packet;
        if (!data_queue.pop(packet)) {
            break;  // 隊列已關閉且為空
        }
        
        // 計算總和
        int64_t isum = 0;
        for (float v : packet.data) {
            isum += static_cast<int64_t>(v * scale);
        }
        
        // 更新結果
        slots[packet.z].sum.fetch_add(isum);
        int c = slots[packet.z].count.fetch_add(1) + 1;
        
        if (c == 4) {
            finished[packet.z] = true;
            completed_count.fetch_add(1);
        }
    }
}

// === Monitor: 使用 condition variable 而非 polling ===
void monitor() {
    int last_count = 0;
    
    while (last_count < Z) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        int current = completed_count.load();
        if (current > last_count) {
            // 處理新完成的 z
            for (int z = 0; z < Z; ++z) {
                if (finished[z].exchange(false)) {  // atomic exchange
                    constexpr double scale = 1e6;
                    double mean = static_cast<double>(slots[z].sum.load()) / (N * 4 * scale);
                    spec_mean[z] = static_cast<size_t>((mean + 1.0) * 1000);
                    std::cout << "z=" << z << " done (progress: " << current << "/256)\n";
                }
            }
            last_count = current;
        }
    }
}

int main(int argc, char** argv) {
    int n_io_threads = 4;     // I/O 線程數
    int n_compute_threads = 4; // 計算線程數
    
    if (argc > 1) n_io_threads = std::stoi(argv[1]);
    if (argc > 2) n_compute_threads = std::stoi(argv[2]);
    
    std::cout << "Starting with " << n_io_threads << " I/O threads and " 
              << n_compute_threads << " compute threads\n";
    std::cout << "Buffer size: 32 packets (~32MB)\n\n";
    
    // 初始化
    for (int i = 0; i < Z; ++i)
        finished[i] = false;
    
    // 填充任務隊列
    for (int z = 0; z < 256; ++z)
        for (int x = 0; x < 2; ++x)
            for (int y = 0; y < 2; ++y)
                task_queue.push({z, x, y});
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 啟動 producer threads (I/O)
    std::vector<std::thread> producers;
    for (int i = 0; i < n_io_threads; ++i)
        producers.emplace_back(producer);
    
    // 啟動 consumer threads (計算)
    std::vector<std::thread> consumers;
    for (int i = 0; i < n_compute_threads; ++i)
        consumers.emplace_back(consumer);
    
    // 啟動 monitor thread
    std::thread mon(monitor);
    
    // 等待所有 producers 完成
    for (auto& t : producers) t.join();
    
    // 通知 data_queue 不再有新數據
    data_queue.set_finished();
    
    // 等待所有 consumers 完成
    for (auto& t : consumers) t.join();
    
    // 等待 monitor 完成
    mon.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    std::cout << "\n=== RESULT ===\n";
    std::cout << "Total time: " << elapsed << " seconds\n";
    std::cout << "Throughput: " << (256 * 4 * 1.0 / elapsed) << " MB/s\n\n";
    
    for (int i = 0; i < 256; ++i)
        std::cout << spec_mean[i] << "\n";
    
    return 0;
}
