#include <iostream>
#include <cstring>
#include <chrono>
#include <pthread.h>
#include "../include/buffer.h"

static constexpr int N_RECORDS = 1000000;
static constexpr int CAPACITY = 1024;

RingBuffer* g_buf;

void* producer(void*) {
    VehicleData item{};
    strncpy(item.internal_id, "SYN001", sizeof(item.internal_id) - 1);
    item.lat = 43.7f;
    item.lon = -79.4f;
    item.speed = 42.0f;
    item.timestamp = 1700000000ULL;

    for (int i = 0; i < N_RECORDS; ++i) {
        buffer_push(g_buf, item);
    }

    return nullptr;
}

int main() {
    // Warm-up: touches memory pages, lets OS scheduler settle
    g_buf = buffer_init(CAPACITY);
    if (!g_buf) {
        std::cerr << "buffer_init failed\n";
        return 1;
    }

    pthread_t warm_tid;
    pthread_create(&warm_tid, nullptr, producer, nullptr);
    VehicleData out{};
    for (int i = 0; i < N_RECORDS; ++i) {
        buffer_pop(g_buf, &out);
    }
    pthread_join(warm_tid, nullptr);
    buffer_destroy(g_buf);

    // Timed run
    g_buf = buffer_init(CAPACITY);
    if (!g_buf) {
        std::cerr << "buffer_init failed\n";
        return 1;
    }

    auto t0 = std::chrono::steady_clock::now();

    pthread_t tid;
    pthread_create(&tid, nullptr, producer, nullptr);
    for (int i = 0; i < N_RECORDS; ++i) {
        buffer_pop(g_buf, &out);
    }

    auto t1 = std::chrono::steady_clock::now();
    pthread_join(tid, nullptr);

    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    double throughput = N_RECORDS / elapsed;

    std::cout << "=== Ring Buffer Throughput Benchmark ===\n";
    std::cout << "Records   : " << N_RECORDS << "\n";
    std::cout << "Capacity  : " << CAPACITY  << "\n";
    std::cout << "Elapsed   : " << elapsed   << " s\n";
    std::cout << "Throughput: " << static_cast<long long>(throughput) << " records/sec\n";

    buffer_destroy(g_buf);
    return 0;
}
