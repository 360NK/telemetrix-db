#include <iostream>
#include <atomic>
#include <cstdint>
#include <new>

#include "../include/buffer.h"

// 1. Naturally Aligned Memory Representation (484 bytes)
struct TimeWindow {
    uint32_t timestamps[60]; // 240 bytes
    float speeds[60];        // 240 bytes
    uint8_t head;            // 1 byte
    uint8_t padding[3];      // 3 bytes (Explicitly filling alignment gap)
};

// 2. Cache-Line Aligned Bucket (512 bytes)
struct alignas(64) HexBucket {
    uint64_t h3_index;       // 8 bytes (0 = empty slot)
    TimeWindow window;       // 484 bytes
    std::atomic_flag lock = ATOMIC_FLAG_INIT; // 1 byte spinlock
    // Compiler auto-pads 19 bytes here to reach 512 bytes
};

static_assert(sizeof(TimeWindow) == 484, "TimeWindow must be 484 bites.");
static_assert(alignof(HexBucket) == 64, "HexBucket must be 64-byte aligned");
static_assert(sizeof(HexBucket) == 512, "HexBucket must be exactly 512 bytes");

// GTHA megaregion needs ~82k H3(Res 9) hexes.
// Sized to 131,072 (2^17) to replace modulo division (~20 cycles) 
// with bitwise AND (1 cycle) during insertion: hash & (BUCKET_COUNT - 1).
constexpr size_t BUCKET_COUNT = 131072; // GTHA Megaregion size

int main() {
    std::cout << "[SYSTEM] Booting Telemetrix-DB Reactor Core...\n";

    // Allocate the unified lock-free memory arena on the heap once.
    HexBucket* database_arena = new (std::align_val_t(64)) HexBucket[BUCKET_COUNT];

    std::cout << "[MEMORY] Arena allocated: " 
            << (sizeof(HexBucket) * BUCKET_COUNT) / (1024 * 1024) 
            << " MB allocated with 64-byte cache alignment.\n";

    RingBuffer *engine_buffer = buffer_init(1024);
    if (!engine_buffer) {
        std::cerr << "[FATAL] Failed to allocate C Ring Buffer.\n";
        return 1;
    }
    std::cout << "[BRIDGE] C Ring Buffer successfully initialized from C++.\n";

    buffer_destroy(engine_buffer);
    operator delete[](database_arena, std::align_val_t(64));

    std::cout << "[SYSTEM] Shutdown complete.\n";
    return 0;
}
