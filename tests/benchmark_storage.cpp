#include <iostream>
#include <chrono>
#include "../include/storage.hpp"

int main() {
    SpatialArena arena;

    constexpr int    N        = 1000000;
    constexpr double LAT_BASE = 43.65;
    constexpr double LON_BASE = -79.60;
    constexpr double LAT_STEP = 0.0002;
    constexpr double LON_STEP = 0.0002;

    if (!arena.update(LAT_BASE, LON_BASE, 0.0f, 0)) {
        std::cerr << "Pre-flight failed: H3 rejected base coordinates\n";
        return 1;
    }

    // Warm-up: pages arena memory, primes H3 internals
    for (int i = 0; i < 100000; ++i) {
        arena.update(
            LAT_BASE + (i % 1000) * LAT_STEP,
            LON_BASE + (i % 1000) * LON_STEP,
            42.0f,
            1700000000ULL + i
        );
    }

    // Timed run
    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        arena.update(
            LAT_BASE + (i % 1000) * LAT_STEP,
            LON_BASE + (i % 1000) * LON_STEP,
            42.0f,
            1700000000ULL + i
        );
    }

    auto t1 = std::chrono::steady_clock::now();

    double elapsed    = std::chrono::duration<double>(t1 - t0).count();
    double throughput = N / elapsed;

    std::cout << "=== Spatial Storage Update Benchmark ===\n";
    std::cout << "Records   : " << N       << "\n";
    std::cout << "Elapsed   : " << elapsed << " s\n";
    std::cout << "Throughput: " << static_cast<long long>(throughput) << " updates/sec\n";
    std::cout << "(single-threaded, uncontested spinlock, 1000 synthetic coordinate pairs)\n";

    return 0;
}
