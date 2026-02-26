#include <iostream>
#include <chrono>
#include <pthread.h>
#include "../include/buffer.h"

constexpr int TOTAL_MESSAGES = 1000000;
RingBuffer* engine_buffer;

void* producer_thread(void* arg) {
    (void)arg;
    
    VehicleData fake_bus = {};

    for (int i = 0; i < TOTAL_MESSAGES; i++) {
        buffer_push(engine_buffer, fake_bus);
    }

    buffer_signal_shutdown(engine_buffer);
    return nullptr;
}

int main() {
    std::cout << "[BENCHMARK] Initializing C Ring Buffer (Capacity: 1024)...\n";
    engine_buffer = buffer_init(1024);

    if (!engine_buffer) {
        std::cerr << "[FATAL] Buffer allocation failed.\n";
        return 1;
    }

    std::cout << "[BENCHMARK] Firing Producer and Consumer threads...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    pthread_t prod_tid;
    pthread_create(&prod_tid, NULL, producer_thread, NULL);

    int consumed_count = 0;
    VehicleData popped_bus;

    while (true) {
        bool is_shut = buffer_is_shutdown(engine_buffer);
        bool got_data = buffer_pop(engine_buffer, &popped_bus);

        if (got_data) {
            consumed_count++;
        } else if (is_shut) {
            break;
        }
    }

    pthread_join(prod_tid, NULL);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    std::cout << "Messages Sent     : " << TOTAL_MESSAGES << "\n";
    std::cout << "Messages Received : " << consumed_count << "\n";
    std::cout << "Time Elapsed      : " << duration.count() << " seconds\n";
    std::cout << "Throughput        : " << (int)(TOTAL_MESSAGES / duration.count()) << " msg/sec\n";

    buffer_destroy(engine_buffer);
    return 0;
}