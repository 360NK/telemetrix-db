# telemetrix-db

A custom in-memory spatial time-series cache for GTFS-Realtime transit 
data.

I originally built the ingestion firehose in pure C to handle raw protobuf 
network streams. This repository merges that pipeline with a C++17 storage 
core to bypass disk I/O and dynamic heap allocation entirely. 

The engine hashes GPS coordinates into Uber H3 hexagons and serves 
time-series queries over a custom TCP socket using a macOS `kqueue` event 
loop. Concurrency is handled via 1-byte atomic spinlocks per bucket, 
guaranteeing the network ingestion thread is never blocked by TCP readers.

### Build Requirements
Requires `protobuf-c` and `h3` to be installed (easily grabbed via 
Homebrew on macOS). 

Compile the unified binary via:
make
