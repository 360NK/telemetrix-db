# telemetrix-db

An in-memory spatial telemetry cache built in hybrid C11/C++17, designed for
bounded ingestion, fixed-size spatial storage, and fine-grained synchronization.

Uses live GTFS-Realtime transit vehicle data as the ingestion workload. The
storage engine is intentionally general — any spatial telemetry source can be
routed into the H3-indexed arena.

> Volatile in-memory cache, not a persistent database. No WAL, no disk writes,
> no durability guarantees.

---

## Architecture & Data Flow

```
Live GTFS-Realtime endpoint
        │
        ▼
src/ingestion/fetcher.c          libcurl network fetch
        │
        ▼
src/ingestion/parser.c           protobuf-c decode → VehicleData
        │
        ▼
src/ingestion/buffer.c           bounded POSIX ring buffer (backpressure)
        │
        ▼
src/storage/storage.cpp          SpatialArena::update()
        ├── lat/lon → H3 cell (resolution 9)
        ├── H3 index → bitmask arena index
        ├── per-bucket spinlock (acquire/release)
        └── rolling speed/timestamp window write
```

Two decoupled layers connected by a bounded ring buffer:

- **Ingestion (C11):** Fetches binary GTFS-Realtime payloads over HTTP,
  decodes with `protobuf-c`, and flattens vehicle positions into `VehicleData`
  records pushed into the ring buffer.
- **Bounded queue:** Fixed-capacity POSIX ring buffer with mutex and condition
  variables. Producer blocks when full — backpressure is intentional.
- **Spatial storage (C++17):** `SpatialArena::update()` converts coordinates
  to H3 cells at resolution 9, routes by bitmask index into a preallocated
  bucket array, acquires a per-bucket spinlock, and writes into a bounded
  rolling window.

---

## Memory Layout & Synchronization

- **Preallocated arena:** 131,072 `HexBucket` entries allocated once at startup
  with 64-byte alignment. No heap allocation in the storage update path after
  initialization.
- **Arena size:** 131,072 × 512 bytes = **64 MB**, fixed for process lifetime.
- **Cache-line alignment:** `HexBucket` is `alignas(64)` and exactly 512 bytes
  (8 cache lines). Adjacent buckets do not share cache lines, reducing
  bucket-to-bucket false sharing under concurrent access.
- **Per-bucket spinlock:** Each bucket contains a `std::atomic_flag` spinlock
  with acquire/release semantics. Lock scope is per-bucket — no global storage
  lock contention.
- **Rolling window:** Each bucket stores the 60 most recent speed and timestamp
  values in a circular array.
- **Bitmask routing:** `H3Index & (BUCKET_COUNT - 1)` — one bitwise AND, O(1).
  Power-of-two bucket count is load-bearing for this.

---

## Benchmarks

Synthetic benchmarks — no network, no protobuf decode, no live feed
variability. Each benchmark isolates one component.

**Ring buffer throughput** — bounded POSIX producer-consumer queue:

| Records | Producer threads | Consumer threads | Capacity | Throughput |
|---------|-----------------|-----------------|----------|------------|
| 1,000,000 | 1 | 1 | 1,024 | **~3.9 million records/sec** |

**Spatial storage update throughput** — full `SpatialArena::update()` path:

| Records | Threads | Coordinate pairs | Throughput |
|---------|---------|-----------------|------------|
| 1,000,000 | 1 | 1,000 synthetic | **~1.4 million updates/sec** |

The storage benchmark measures H3 coordinate conversion, bucket index
selection, uncontested per-bucket lock/unlock, and rolling-window writes.
Storage throughput is likely dominated by H3 coordinate conversion, since each
update calls `latLngToCell` before bucket selection and window writes. It does
not measure network fetch, protobuf parsing, live GTFS variability, or
multi-writer contention.

*Environment: 2017 MacBook Air, macOS, clang/clang++ with `-O3`, median of 6 local runs per benchmark.*

---

## Source Layout

```
src/
  ingestion/
    fetcher.c                 libcurl network ingestion
    parser.c                  protobuf-c decode → VehicleData
    buffer.c                  bounded POSIX ring buffer implementation
    gtfs-realtime.pb-c.c      generated protobuf-c implementation (tracked)
  storage/
    storage.cpp               SpatialArena implementation
  main.cpp                    orchestrator: boot, ingestion thread, consumer loop

include/
  buffer.h                    VehicleData, RingBuffer — C ABI (extern "C" guarded)
  arena.h                     TimeWindow, HexBucket, BUCKET_COUNT
  storage.hpp                 SpatialArena class declaration
  gtfs-realtime.pb-c.h        generated protobuf-c header (tracked)

tests/
  benchmark_buffer.cpp        ring buffer throughput benchmark
  benchmark_storage.cpp       storage update throughput benchmark

proto/
  gtfs-realtime.proto         GTFS-Realtime protobuf schema
```

---

## Build

**Dependencies:** `libcurl`, `protobuf-c`, `h3`

```bash
make              # build all targets
make clean        # remove build artifacts
```

| Binary | Description |
|--------|-------------|
| `bin/telemetrix-db` | Live ingestion and spatial storage (requires network) |
| `bin/benchmark_buffer` | Synthetic ring buffer throughput benchmark |
| `bin/benchmark_storage` | Synthetic storage update throughput benchmark |

---

## Trade-offs & Limitations

- **Volatile cache:** No disk persistence. Process crash loses all cached state.
- **Bounded backpressure:** If the consumer lags, the ring buffer fills and the
  producer blocks. Memory usage stays bounded — intentional behavior.
- **Lossy hash routing:** Multiple H3 cells can alias to the same bucket through
  bitmask routing. The bucket records the most recent `h3_index`, but aliased
  cells may overwrite or mix recent-window samples until collision handling is
  added.
- **Recent history, not ordered events:** Window entries reflect recent writes
  received, not globally ordered by event timestamp.
- **Single consumer baseline:** Current architecture uses one consumer thread.
  Per-bucket spinlock design supports multi-worker extension, but contention
  behavior under concurrent writers is not yet measured.
- **GTFS adapter only:** `VehicleData` is GTFS-specific. A generic telemetry
  record boundary is planned before adding new adapters.

---

## Roadmap

- [x] Bounded producer-consumer ring buffer with backpressure
- [x] H3-indexed preallocated spatial arena (64 MB, 131,072 buckets)
- [x] Per-bucket `std::atomic_flag` synchronization with acquire/release semantics
- [x] `SpatialArena` storage module extraction (RAII, clean C++17 interface)
- [x] Synthetic benchmark suite for queue and storage update throughput
- [ ] Multi-worker consumer with contention benchmarking (hotspot vs. uniform)
- [ ] Minimal query/debug interface (bucket reads, window inspection)
- [ ] Generic telemetry record boundary (decouple from GTFS `VehicleData`)
- [ ] Automated `protoc-c` generation in Makefile
