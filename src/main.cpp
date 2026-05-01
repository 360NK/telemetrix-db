#include <iostream>
#include <atomic>
#include <cstdint>
#include <new>
#include <thread>
#include <chrono>
#include <pthread.h>
#include <h3/h3api.h> // Uber's Spatial Library

#include <unordered_map>
#include <fstream>
#include <sstream>
#include <string>

#include "../include/buffer.h"
#include "../include/arena.h"

extern "C" {
    uint8_t* fetch_gtfs_data(const char* url, size_t* out_size);
    void parse_and_queue(uint8_t* buffer, size_t len, RingBuffer* rb);
}

static_assert(sizeof(TimeWindow) == 484, "TimeWindow must be 484 bites.");
static_assert(alignof(HexBucket) == 64, "HexBucket must be 64-byte aligned");
static_assert(sizeof(HexBucket) == 512, "HexBucket must be exactly 512 bytes");

std::unordered_map<std::string, std::string> stop_names;
std::unordered_map<std::string, std::string> route_names;

void load_static_gtfs() {
    std::cout << "[SYSTEM] Loading Static GTFS Dictionaries...\n";
    
    // Helper lambda to strip double-quotes from strings
    auto strip_quotes = [](std::string& str) {
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
            str = str.substr(1, str.size() - 2);
        }
    };

    // Load Stops
    std::ifstream stops_file("TTC Routes and Schedules Data/stops.txt");
    std::string line, id, name, ignore;
    if (stops_file.is_open()) {
        std::getline(stops_file, line); // skip header
        while (std::getline(stops_file, line)) {
            std::stringstream ss(line);
            std::getline(ss, id, ',');
            std::getline(ss, ignore, ','); // skip stop_code
            std::getline(ss, name, ','); 
            
            // Clean both the ID and the Name
            strip_quotes(id);
            strip_quotes(name);
            stop_names[id] = name;
        }
    } else {
        std::cerr << "[WARNING] stops.txt not found.\n";
    }

    // Load Routes
    std::ifstream routes_file("TTC Routes and Schedules Data/routes.txt");
    if (routes_file.is_open()) {
        std::getline(routes_file, line); // skip header
        while (std::getline(routes_file, line)) {
            std::stringstream ss(line);
            std::getline(ss, id, ',');
            std::getline(ss, ignore, ','); // skip agency_id
            std::getline(ss, ignore, ','); // skip route_short_name (e.g., "60")
            std::getline(ss, name, ',');   // get route_long_name (e.g., "Steeles West")
            
            // Clean both the ID and the Name
            strip_quotes(id);
            strip_quotes(name);
            route_names[id] = name;
        }
    } else {
         std::cerr << "[WARNING] routes.txt not found.\n";
    }
}

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

    const char* ttc_url = "https://bustime.ttc.ca/gtfsrt/vehicles";
    // Helsinki, Finland
    // const char* ttc_url = "https://realtime.hsl.fi/realtime/vehicle-positions/v2/hsl";

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

    load_static_gtfs();

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
                // Safe lookups (fallback to ID if not found in map)
                std::string r_id(popped_bus.route_id);
                std::string s_id(popped_bus.stop_id);
                std::string route_display = route_names.count(r_id) ? route_names[r_id] : r_id;
                std::string stop_display = stop_names.count(s_id) ? stop_names[s_id] : s_id;

                std::cout << "[PASSENGERS DETECTED] \n";
                std::cout << "[CORE] Bus " << popped_bus.fleet_number 
                          << " (Route " << route_display << ") | "
                          << "Speed: " << popped_bus.speed << " km/h | "
                          << "Load: " << get_occupancy_string(popped_bus.occupancy_status) << " | "
                          << "Percent Load: " << popped_bus.occupancy_percentage << "% | "
                          << "Bearing: " << popped_bus.bearing << "° | "
                          << "Heading to Stop: " << stop_display << "\n";
            }
        }
    }

    buffer_signal_shutdown(engine_buffer);
    pthread_join(ing_tid, NULL);
    buffer_destroy(engine_buffer);
    operator delete[](database_arena, std::align_val_t(64));

    return 0;
}
