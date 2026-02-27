#ifndef ARENA_H
#define ARENA_H

#include <atomic>
#include <cstdint>

// 1. Naturally Aligned Memory Representation (484 bytes)
struct TimeWindow {
    uint32_t timestamps[60]; 
    float speeds[60];        
    uint8_t head;            
    uint8_t padding[3];      
};

// 2. Cache-Line Aligned Bucket (512 bytes)
struct alignas(64) HexBucket {
    uint64_t h3_index;       
    TimeWindow window;       
    std::atomic_flag lock = ATOMIC_FLAG_INIT; 
};

// 131,072 (2^17) allows for 1-cycle Bitwise AND hashing
constexpr size_t BUCKET_COUNT = 131072; 

#endif
