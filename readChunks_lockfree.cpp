#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <array>

const int Z = 256;
const size_t N = 262144;

// === Lock-Free Queue (Michael-Scott Algorithm) ===
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<Node*> next;
        Node() : next(nullptr) {}
    };
    
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    std::atomic<size_t> size_;
    
public:
    LockFreeQueue() : size_(0) {
        Node* dummy = new Node();
        head_.store(dummy);
        tail_.store(dummy);
    }
    
    ~LockFreeQueue() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next);
            delete old_head;
        }
    }
    
    void push(T value) {
        auto data = std::make_shared<T>(std::move(value));
        Node* new_node = new Node();
        new_node->data = data;
        new_node->next.store(nullptr);
        
        while (true) {
            Node* old_tail = tail_.load();
            Node* old_next = old_tail->next.load();
            
            if (old_tail == tail_.load()) {
                if (old_next == nullptr) {
                    if (old_tail->next.compare_exchange_weak(old_next, new_node)) {
                        tail_.compare_exchange_weak(old_tail, new_node);
                        size_.fetch_add(1);
                        return;
                    }
                } else {
                    tail_.compare_exchange_weak(old_tail, old_next);
                }
            }
        }
    }
    
    bool pop(T& result) {
        while (true) {
            Node* old_head = head_.load();
            Node* old_tail = tail_.load();
            Node* old_next = old_head->next.load();
            
            if (old_head == head_.load()) {
                if (old_head == old_tail) {
                    if (old_next == nullptr) {
                        return false;  // Queue is empty
                    }
                    tail_.compare_exchange_weak(old_tail, old_next);
                } else {
                    if (old_next != nullptr && old_next->data) {
                        result = *old_next->data;
                        if (head_.compare_exchange_weak(old_head, old_next)) {
                            size_.fetch_sub(1);
                            delete old_head;
                            return true;
                        }
                    }
                }
            }
        }
    }
    
    size_t size() const { return size_.load(); }
    bool empty() const { return size_.load() == 0; }
};

// === Data Structures ===
struct Task {
    int z, x, y;
};

struct DataPacket {
    int z, x, y;
    std::vector<float> data;
    
    DataPacket() : z(0), x(0), y(0) {}
    DataPacket(int z_, int x_, int y_) : z(z_), x(x_), y(y_), data(N) {}
};

struct Slot {
    std::atomic<int> count{0};
    std::atomic<int64_t> sum{0};
};

// === Global State ===
LockFreeQueue<Task> task_queue;
LockFreeQueue<DataPacket> data_queue;
std::array<Slot, Z> slots;
std::atomic<bool> finished[Z];
std::vector<size_t> spec_mean(256);
std::atomic<int> completed_count{0};
std::atomic<bool> producers_done{false};
std::atomic<int> active_producers{0};

// === Producer: 讀取檔案 ===
void producer() {
    active_producers.fetch_add(1);
    
    while (true) {
        Task task;
        if (!task_queue.pop(task)) {
            if (task_queue.empty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                if (task_queue.empty()) break;
            }
            continue;
        }
        
        // 背壓控制：如果緩衝區太大，稍微等待
        while (data_queue.size() > 64) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
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
        
        data_queue.push(std::move(packet));
    }
    
    if (active_producers.fetch_sub(1) == 1) {
        producers_done.store(true);
    }
}

// === Consumer: 計算 ===
void consumer() {
    constexpr double scale = 1e6;
    
    while (true) {
        DataPacket packet;
        if (!data_queue.pop(packet)) {
            if (producers_done.load() && data_queue.empty()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            continue;
        }
        
        // 計算總和（可以用 SIMD 優化）
        int64_t isum = 0;
        for (float v : packet.data) {
            isum += static_cast<int64_t>(v * scale);
        }
        
        // Lock-free 更新
        slots[packet.z].sum.fetch_add(isum, std::memory_order_relaxed);
        int c = slots[packet.z].count.fetch_add(1, std::memory_order_relaxed) + 1;
        
        if (c == 4) {
            finished[packet.z].store(true, std::memory_order_release);
            completed_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// === Monitor ===
void monitor() {
    int last_count = 0;
    auto last_time = std::chrono::steady_clock::now();
    
    while (last_count < Z) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        int current = completed_count.load(std::memory_order_relaxed);
        if (current > last_count) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_time).count();
            int delta = current - last_count;
            
            // 處理新完成的 z
            for (int z = 0; z < Z; ++z) {
                if (finished[z].exchange(false, std::memory_order_acquire)) {
                    constexpr double scale = 1e6;
                    double mean = static_cast<double>(slots[z].sum.load(std::memory_order_relaxed)) / (N * 4 * scale);
                    spec_mean[z] = static_cast<size_t>((mean + 1.0) * 1000);
                }
            }
            
            std::cout << "Progress: " << current << "/256 "
                      << "(+" << delta << " in " << elapsed << "s, "
                      << "rate: " << (delta * 4.0 / elapsed) << " MB/s, "
                      << "queue: " << data_queue.size() << ")\n";
            
            last_count = current;
            last_time = now;
        }
    }
}

int main(int argc, char** argv) {
    int n_io_threads = 4;
    int n_compute_threads = 8;
    
    if (argc > 1) n_io_threads = std::stoi(argv[1]);
    if (argc > 2) n_compute_threads = std::stoi(argv[2]);
    
    std::cout << "=== Lock-Free Producer-Consumer ===\n";
    std::cout << "I/O threads: " << n_io_threads << "\n";
    std::cout << "Compute threads: " << n_compute_threads << "\n";
    std::cout << "No locks, no blocking, pure atomic operations!\n\n";
    
    // 初始化
    for (int i = 0; i < Z; ++i)
        finished[i].store(false);
    
    // 填充任務
    for (int z = 0; z < 256; ++z)
        for (int x = 0; x < 2; ++x)
            for (int y = 0; y < 2; ++y)
                task_queue.push({z, x, y});
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 啟動所有線程
    std::vector<std::thread> producers;
    for (int i = 0; i < n_io_threads; ++i)
        producers.emplace_back(producer);
    
    std::vector<std::thread> consumers;
    for (int i = 0; i < n_compute_threads; ++i)
        consumers.emplace_back(consumer);
    
    std::thread mon(monitor);
    
    // 等待完成
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    mon.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    std::cout << "\n=== RESULT ===\n";
    std::cout << "Total time: " << elapsed << " seconds\n";
    std::cout << "Total data: " << (256 * 4) << " MB\n";
    std::cout << "Throughput: " << (256 * 4 * 1.0 / elapsed) << " MB/s\n\n";
    
    for (int i = 0; i < 256; ++i)
        std::cout << spec_mean[i] << "\n";
    
    return 0;
}
