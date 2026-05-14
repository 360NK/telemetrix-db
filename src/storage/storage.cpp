#include "../../include/storage.hpp"

#include <atomic>
#include <cstddef>
#include <new>

#include <h3/h3api.h> // Uber's Spatial Library

static_assert(sizeof(TimeWindow) == 484, "TimeWindow must be 484 bytes.");
static_assert(alignof(HexBucket) == 64, "HexBucket must be 64-byte aligned.");
static_assert(sizeof(HexBucket) == 512, "HexBucket must be exactly 512 bytes.");

namespace {
constexpr int H3_RESOLUTION = 9;
}

SpatialArena::SpatialArena()
    : arena_(new (std::align_val_t(64)) HexBucket[BUCKET_COUNT])
{

}

SpatialArena::~SpatialArena()
{
    operator delete[](arena_, std::align_val_t(64));
}

bool SpatialArena::update(double lat, double lon, float speed, uint64_t timestamp)
{
    LatLng geo = {degsToRads(lat), degsToRads(lon)};

    H3Index hex_id;
    if (latLngToCell(&geo, H3_RESOLUTION, &hex_id) != 0) {
        return false;
    }

    const std::size_t arena_idx = hex_id & (BUCKET_COUNT - 1);
    HexBucket& bucket = arena_[arena_idx];

    while (bucket.lock.test_and_set(std::memory_order_acquire)) {}

    bucket.h3_index = hex_id;
    const uint8_t head = bucket.window.head;
    bucket.window.speeds[head] = speed;
    bucket.window.timestamps[head] = static_cast<uint32_t>(timestamp);
    bucket.window.head = (head + 1) % 60;

    bucket.lock.clear(std::memory_order_release);
    return true;
}
