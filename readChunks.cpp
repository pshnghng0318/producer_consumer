#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

const int Z = 256;
const int FILES_PER_Z = 4;
const size_t N = 262144;

struct Task {
    int z, x, y;
};

struct Slot {
    std::atomic<int> count{0};
    std::atomic<int64_t> sum{0}; // lock-free 累加
};

std::queue<Task> task_queue;
std::mutex mtx;
std::condition_variable cv;
bool done = false;

Slot slots[Z];
std::atomic<bool> finished[Z];

std::vector<size_t> spec_mean(256);

int64_t read_file_sum_scaled(const std::string& fname) {
    std::ifstream in(fname, std::ios::binary);
    std::vector<float> buf(N);
    in.read(reinterpret_cast<char*>(buf.data()), N * sizeof(float));

    int64_t isum = 0;
    constexpr double scale = 1e6;
    for (float v : buf) isum += static_cast<int64_t>(v * scale);
    return isum;
}

// worker threads
void worker() {
    while (true) {
        Task t;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !task_queue.empty() || done; });

            if (done && task_queue.empty())
                return;

            t = task_queue.front();
            task_queue.pop();
        }

        std::string fname = "data/" +
            std::to_string(t.z) + "." +
            std::to_string(t.x) + "." +
            std::to_string(t.y);

        int64_t isum = read_file_sum_scaled(fname);

        slots[t.z].sum.fetch_add(isum);
        int c = slots[t.z].count.fetch_add(1) + 1;

        if (c == 4) {
            finished[t.z] = true;
        }
    }
}

// monitor thread
void monitor() {
    int done_count = 0;

    while (done_count < Z) {
        for (int z = 0; z < Z; ++z) {
            if (finished[z]) {
                constexpr double scale = 1e6;
                double mean = static_cast<double>(slots[z].sum.load()) / (N * 4 * scale);
                spec_mean[z] = static_cast<size_t>((mean + 1.0) * 1000); // example mapping

                finished[z] = false; // prevent double count
                done_count++;

                std::cout << "z=" << z << " done\n";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    done = true;
    cv.notify_all();
}

int main(int argc, char** argv) {
    int n_threads = 8;
    if (argc > 1) n_threads = std::stoi(argv[1]);

    // init flags
    for (int i = 0; i < Z; ++i)
        finished[i] = false;

    // enqueue tasks
    for (int z = 0; z < 256; ++z)
        for (int x = 0; x < 2; ++x)
            for (int y = 0; y < 2; ++y)
                task_queue.push({z, x, y});

    // start threads
    std::vector<std::thread> workers;
    for (int i = 0; i < n_threads; ++i)
        workers.emplace_back(worker);

    std::thread mon(monitor);

    cv.notify_all();

    for (auto& t : workers) t.join();
    mon.join();

    std::cout << "\n=== RESULT ===\n";
    for (int i = 0; i < 256; ++i)
        std::cout << spec_mean[i] << "\n";
}
