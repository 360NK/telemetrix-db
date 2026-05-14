#ifndef STORAGE_HPP
#define STORAGE_HPP

#include <cstdint>

#include "arena.h"

class SpatialArena {
public:
    SpatialArena();
    ~SpatialArena();

    SpatialArena(const SpatialArena&) = delete;
    SpatialArena& operator=(const SpatialArena&) = delete;

    bool update(double lat, double lon, float speed, uint64_t timestamp);

private:
    HexBucket* arena_;
};

#endif