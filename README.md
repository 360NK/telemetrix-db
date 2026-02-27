# telemetrix-db

An in-memory spatial telemetry cache for GTFS-Realtime ingestion, spatial bucketing, and rolling time-window aggregation.

This project explores hybrid C/C++ systems programming, focusing on cache-aware memory layout, bounded backpressure, and fine-grained synchronization between an ingestion pipeline and a fixed-size spatial storage arena.

It is designed as a fast, in-memory cache for localized recent telemetry, not as a durable time-series database.

## Architecture & Data Flow

The system is split into two decoupled layers connected by a bounded ring buffer:

- **Ingestion layer (C11):** Fetches binary GTFS-Realtime payloads, decodes them with `protobuf-c`, and extracts flattened vehicle telemetry records.
- **Bounded queue:** A fixed-capacity producer/consumer ring buffer uses POSIX mutexes and condition variables to manage backpressure between ingestion and storage.
- **Spatial arena (C++17):** A consumer thread maps coordinates into Uber H3 cells and routes them into a preallocated fixed-size arena by masking the H3 index into a bounded bucket array.
- **Rolling window aggregation:** Each spatial bucket stores a bounded recent-history window of speed/timestamp updates for that cell.

## Memory Layout & Synchronization

The storage path relies on fixed-size, preallocated structures to keep updates predictable and cache-aware.

- **Preallocated arena:** The core storage arena is allocated once at startup, avoiding dynamic allocation in the main storage update loop.
- **Cache-line alignment:** `HexBucket` is 64-byte aligned and sized to 512 bytes, so each bucket occupies 8 cache lines and adjacent buckets do not share a cache-line boundary.
- **Per-bucket synchronization:** Each bucket contains a localized `std::atomic_flag` spinlock using acquire/release semantics, keeping synchronization scoped to individual buckets rather than a single global storage lock.

## Trade-offs & Limitations

This project is a volatile ingestion cache, not a persistent database.

- **Bounded backpressure:** If the consumer lags, the ring buffer fills and the producer blocks until space becomes available.
- **Lossy hash routing:** Bucket indices are derived from H3 IDs via a bitmask. If multiple cells map to the same bucket, the current strategy is latest-writer-wins.
- **Recent-write history, not strict event ordering:** Bucket windows reflect recent writes received for a spatial cell and are not guaranteed to be globally ordered by event timestamp.
- **In-memory only:** There is no WAL or disk persistence; a process crash loses cached state.

## Benchmarking Status

Live GTFS feeds are used to validate ingestion and parsing correctness only.

Synthetic benchmarking is being used to characterize:

- queue throughput
- producer backpressure
- hotspot vs. uniform spatial workload behavior
- bounded memory usage

## Build

Requires `protobuf-c` and `h3`.

```bash
make
./bin/telemetrix-db
```

## Roadmap

- add a reproducabible synthetic benchmark harness
- measure queue-only and full-storage-path throughput
- add a minimal query/debug interface after benchmarking
