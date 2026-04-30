#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <filesystem>

int main() {
    std::filesystem::create_directory("data");

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    const size_t N = 262144; // 1MB / sizeof(float)

    std::vector<float> buffer(N);

    for (int z = 0; z < 256; ++z) {
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                for (size_t i = 0; i < N; ++i)
                    buffer[i] = dist(rng);

                std::string fname = "data/" +
                    std::to_string(z) + "." +
                    std::to_string(x) + "." +
                    std::to_string(y);

                std::ofstream out(fname, std::ios::binary);
                out.write(reinterpret_cast<char*>(buffer.data()),
                          N * sizeof(float));
            }
        }
    }

    std::cout << "Done generating files\n";
}
