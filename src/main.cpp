#include <iostream>
#include <atomic>
#include <cstdint>
#include <new>
#include <thread>
#include <chrono>
#include <pthread.h>
#include <h3/h3api.h> // Uber's Spatial Library

#include "../include/buffer.h"
#include "../include/arena.h"

extern "C" {
    uint8_t* fetch_gtfs_data(const char* url, size_t* out_size);
    void parse_and_queue(uint8_t* buffer, size_t len, RingBuffer* rb);
}

static_assert(sizeof(TimeWindow) == 484, "TimeWindow must be 484 bites.");
static_assert(alignof(HexBucket) == 64, "HexBucket must be 64-byte aligned");
static_assert(sizeof(HexBucket) == 512, "HexBucket must be exactly 512 bytes");

RingBuffer *engine_buffer;

const char* get_occupancy_string(int status) {
    switch(status) {
        case 0: return "EMPTY (0)";
        case 1: return "MANY SEATS (1)";
        case 2: return "FEW SEATS (2)";
        case 3: return "STANDING ROOM ONLY (3)";
        case 4: return "CRUSHED STANDING (4)";
        case 5: return "FULL (5)";
        case 6: return "NOT ACCEPTING PASSENGERS (6)";
        case 7: return "NO DATA (7)";
        case 8: return "NOT BOARDABLE (8)";
        default: return "UNKNOWN";
    }
}

void *ingestion_thread(void* arg) {
    (void)arg;

    // const char* ttc_url = "https://bustime.ttc.ca/gtfsrt/vehicles";
    // Helsinki, Finland
    const char* ttc_url = "https://realtime.hsl.fi/realtime/vehicle-positions/v2/hsl";

    while (!buffer_is_shutdown(engine_buffer)){
        size_t payload_size = 0;

        std::cout << "[NETWORK] Fetching live GTFS-Realtime payload...\n";
        uint8_t* raw_protobuf = fetch_gtfs_data(ttc_url, &payload_size);

        if (raw_protobuf != nullptr && payload_size > 0) {
            parse_and_queue(raw_protobuf, payload_size, engine_buffer);
            free(raw_protobuf);
        } else {
            std::cerr << "[NETWORK] WARNING: Failed to fetch data. Retrying next tick.\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(15));
    }
    return nullptr;
}

int main() {
    std::cout << "[SYSTEM] Booting Telemetrix-DB Reactor Core...\n";

    // Allocate the unified lock-free memory arena on the heap once.
    HexBucket* database_arena = new (std::align_val_t(64)) HexBucket[BUCKET_COUNT];

    std::cout << "[MEMORY] Arena allocated: " 
            << (sizeof(HexBucket) * BUCKET_COUNT) / (1024 * 1024) 
            << " MB allocated with 64-byte cache alignment.\n";

    engine_buffer = buffer_init(1024);
    if (!engine_buffer) {
        std::cerr << "[FATAL] Failed to allocate C Ring Buffer.\n";
        return 1;
    }

    pthread_t ing_tid;
    pthread_create(&ing_tid, NULL, ingestion_thread, NULL);

    std::cout << "[SYSTEM] Core online. Awaiting spatial data...\n";

    VehicleData popped_bus = {};
    uint64_t total_processed = 0;

    while (true) {
        bool got_data = buffer_pop(engine_buffer, &popped_bus);

        if (got_data) {
            if (strncmp(popped_bus.internal_id, "UNKNOWN", sizeof(popped_bus.internal_id)) == 0) {
                continue; 
            }

            LatLng geo = { degsToRads(popped_bus.lat), degsToRads(popped_bus.lon) };
            H3Index hex_id;
            latLngToCell(&geo, 9, &hex_id);

            size_t arena_idx = hex_id & (BUCKET_COUNT - 1);
            HexBucket& target_bucket = database_arena[arena_idx];

            while (target_bucket.lock.test_and_set(std::memory_order_acquire)) {
                // Spin wait...
            }

            target_bucket.h3_index = hex_id; 
            uint8_t head = target_bucket.window.head;

            target_bucket.window.speeds[head] = popped_bus.speed;
            // dummy timestamp
            target_bucket.window.timestamps[head] = 123456789;

            target_bucket.window.head = (head + 1) % 60;

            target_bucket.lock.clear(std::memory_order_release);
            
            total_processed++;

            // if (total_processed % 500 == 0) {
            //     std::cout << "[CORE] Bus " << popped_bus.fleet_number 
            //               << " (Route " << popped_bus.route_id << ") | "
            //               << "Speed: " << popped_bus.speed << " km/h | "
            //               << "Load: " << get_occupancy_string(popped_bus.occupancy_status) << " | "
            //               << "Bearing: " << popped_bus.bearing << "° | "
            //               << "Heading to Stop: " << popped_bus.stop_id << "\n";
            // }

            if (popped_bus.occupancy_status > 1 && popped_bus.occupancy_status  < 6) {
                std::cout << "[PASSENGERS DETECTED] \n";
                std::cout << "[CORE] Bus " << popped_bus.fleet_number 
                          << " (Route " << popped_bus.route_id << ") | "
                          << "Speed: " << popped_bus.speed << " km/h | "
                          << "Load: " << get_occupancy_string(popped_bus.occupancy_status) << " | "
                          << "Percent Load" << popped_bus.occupancy_percentage << "% | "
                          << "Bearing: " << popped_bus.bearing << "° | "
                          << "Heading to Stop: " << popped_bus.stop_id << "\n";
            }
        }
    }

    buffer_signal_shutdown(engine_buffer);
    pthread_join(ing_tid, NULL);
    buffer_destroy(engine_buffer);
    operator delete[](database_arena, std::align_val_t(64));

    return 0;
}
