#include <iostream>
#include <chrono>
#include <pthread.h>
#include <stdint.h>

#include "../include/buffer.h"

extern "C" {
    uint8_t* fetch_gtfs_data(const char* url, size_t* out_size);
    void parse_and_queue(uint8_t* buffer, size_t len, RingBuffer* rb);
}

RingBuffer* engine_buffer;

void* producer_thread(void* arg) {
    (void)arg; 

    const char* ttc_url = "https://bustime.ttc.ca/gtfsrt/vehicles";
    size_t payload_size = 0;

    std::cout << "[PRODUCER] Reaching out to TTC Servers...\n";
    
    uint8_t* raw_protobuf = fetch_gtfs_data(ttc_url, &payload_size);

    if (raw_protobuf != nullptr && payload_size > 0) {
        std::cout << "[PRODUCER] Success! Downloaded " << payload_size << " bytes.\n";
        std::cout << "[PRODUCER] Handing binary blob to the Protobuf Parser...\n";
        
        parse_and_queue(raw_protobuf, payload_size, engine_buffer);
    } else {
        std::cerr << "[PRODUCER] ERROR: Failed to download TTC data.\n";
    }
    
    buffer_signal_shutdown(engine_buffer);
    return nullptr;
}

int main() {
    std::cout << "[SYSTEM] Booting Live TTC Data Bridge...\n";
    engine_buffer = buffer_init(1024);

    if (!engine_buffer) {
        std::cerr << "[FATAL] Buffer allocation failed.\n";
        return 1;
    }

    pthread_t prod_tid;
    pthread_create(&prod_tid, NULL, producer_thread, NULL);

    int consumed_count = 0;
    VehicleData popped_bus = {};
    
    std::cout << "\n--- INCOMING TTC BUSES ---\n";

    while (true) {
        bool is_shut = buffer_is_shutdown(engine_buffer);
        bool got_data = buffer_pop(engine_buffer, &popped_bus);
        
        if (got_data) {
            consumed_count++;
            if (consumed_count <= 10) {
                std::cout << " Bus ID: " << popped_bus.internal_id 
                          << " | Route: " << popped_bus.route_id 
                          << " | Speed: " << popped_bus.speed << " km/h\n";
            }
        } else if (is_shut) {
            break; 
        }
    }

    pthread_join(prod_tid, NULL);

    std::cout << "--------------------------\n";
    std::cout << "[SYSTEM] Bridge Closed. Total real buses processed: " << consumed_count << "\n";

    buffer_destroy(engine_buffer);
    return 0;
}