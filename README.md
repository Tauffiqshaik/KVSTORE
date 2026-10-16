# kvstore — Sharded Key-Value Store with Crash Recovery (C++17)

A thread-safe, sharded, persistent key-value store written in C++17. Data is spread across shards using **consistent hashing**, and every write is logged to a **write-ahead log (WAL)** so the store recovers its state after a crash or restart.

## Features
- **Consistent hashing with virtual nodes** — keys are spread evenly across shards, and resharding moves only a small fraction of keys.
- **Write-ahead log** — each shard appends every `put`/`delete` to its own log file before updating memory; on startup the log is replayed.
- **Crash safety** — a truncated (torn) last record from a crash is detected and ignored during recovery.
- **Log compaction** — rewrites a shard's log with only live entries and swaps it in with an atomic file rename.
- **Concurrency** — one mutex per shard, so threads working on different shards never block each other.
- **Binary-safe** — length-prefixed records, so keys and values can contain spaces, newlines and null bytes.

## Architecture
```
            put/get/del(key)
                  |
        +---------v----------+
        |  Store             |   consistent-hash ring (FNV-1a, 64 virtual nodes per shard)
        +----+-----+-----+---+
             |     |     |
          Shard0 Shard1 ... ShardN      each: hash map in memory + append-only WAL file
             |
        shardN.wal   record format:  <P|D> <keylen> <vallen>\n<key><value>\n
```

## Build and run
Requires `g++` with C++17 support and `make`.
```bash
make test     # builds and runs the unit tests
make bench    # builds and runs the throughput benchmark
```

## Usage
```cpp
#include "kv.hpp"

kv::Store store("/tmp/mydata", /*num_shards=*/8);
store.put("user:1", "alice");
auto v = store.get("user:1");     // std::optional<std::string>
store.del("user:1");
store.compact();                  // shrink the logs
```

## Tests
`test.cpp` covers: overwrite and delete, recovery after restart, binary-safe values, compaction, recovery from a truncated log record, 8-thread concurrent writes, and shard balance.

## Benchmark
Run `make bench` and record the results from your own machine here:

| Operation | Threads | Ops/sec |
|-----------|---------|---------|
| PUT       | 4       | _fill in_ |
| GET       | 4       | _fill in_ |

Note: writes are flushed to the OS buffer but not `fsync`ed, so this trades some durability for speed.

## Limitations and future work
- Runs on a single machine; there is no network layer or replication yet.
- Add a TCP server with a get/put/delete protocol.
- Replicate each shard to a second node and handle node failure.
- Add `fsync` as a durability option, and a memory-efficient index for very large datasets.
