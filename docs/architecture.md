# Architecture Documentation

## Overview

This is a distributed in-memory key-value store designed to demonstrate both low-level systems programming (C++) and distributed systems expertise (Go). The project follows a polyglot architecture where performance-critical operations run in C++ while cluster coordination happens in Go.

## Current Implementation (Phase 3 Complete)

### C++ Storage Engine

The storage engine is built in C++17 and provides the core data storage and retrieval functionality.

#### Components

**1. Hash Table (`engine/include/hash_table.h`)**
- Custom implementation with chaining for collision resolution
- Dynamic resizing at 0.75 load factor
- MurmurHash3-inspired hash function
- O(1) average case for insert/get/delete

**2. Eviction Policies**
- **LRU** (`engine/include/lru_policy.h`): Doubly-linked list with O(1) access updates
- **LFU** (`engine/include/lfu_policy.h`): Min-heap with lazy deletion
- Configurable at HashTable creation time

**3. TTL Expiration**
- Lazy expiry: checked on GET operations
- Active expiry: background scanner thread (100ms interval)
- Configurable scan frequency

**4. Concurrency Model**

Two implementations provided for comparison:

- **ShardedEngineSync** (`engine/include/sharded_engine_sync.h`): Fine-grained per-shard locking
  - N shards = N mutexes
  - Deterministic routing: `shard_id = hash(key) % N`
  - Synchronous API for fair benchmarking

- **MutexEngine** (`engine/include/mutex_engine.h`): Global mutex baseline
  - Single mutex protecting all operations
  - Used for performance comparison

**Performance**: Fine-grained locking achieves 2.82x-3.90x advantage over global mutex (4-8 threads).

**5. Persistence Layer**

**Write-Ahead Log** (`engine/include/wal.h`)
- Binary format: `[op_type:1][key_len:4][key:var][value_len:4][value:var][ttl:8]`
- Batched writes (default 100 ops per batch)
- fsync after each batch for durability
- Handles partial writes gracefully during recovery

**Snapshots** (`engine/include/snapshot.h`)
- Magic number + CRC64 checksum (ECMA-182 polynomial)
- Full serialization including metadata (access_count, last_access_ns, expiry_ns)
- WAL truncation after successful snapshot
- Recovery: Load snapshot + replay WAL

## Design Decisions

### Why Custom Hash Table?

**Rationale:**
1. Educational value for interviews (shows understanding of fundamentals)
2. Full control over memory layout and resizing strategy
3. Optimized for our specific workload (key-value storage)
4. Avoids std::unordered_map overhead

**Trade-offs:**
- More code to maintain vs using STL
- Need to implement resizing, collision handling ourselves
- BUT: Complete control and interview-defensible design

### Why Thread-Per-Core (Shared-Nothing)?

**Rationale:**
1. Eliminates lock contention on hot path
2. Near-linear scaling with core count
3. Cache-friendly (each thread owns its data)
4. Inspired by modern systems (Dragonfly, ScyllaDB)

**Implementation:**
- Deterministic key routing: `hash(key) % num_threads`
- Each thread has exclusive HashTable instance
- No cross-thread synchronization for reads/writes

**Measured Performance:**
- 4 threads: 2.95x speedup (shared-nothing async)
- 8 threads: 4.77x speedup (shared-nothing async)
- Fine-grained sync: 2.82x-3.90x vs global mutex

### Why WAL + Snapshots?

**Rationale:**
1. **Durability**: WAL ensures committed data survives crashes
2. **Fast Recovery**: Snapshots bound recovery time
3. **Real-World Pattern**: Used by Postgres, RocksDB, etc.

**Trade-offs:**
- **Write Throughput**: fsync adds latency (~1-10ms per batch)
- **Storage**: WAL grows until snapshot
- **Complexity**: Need to coordinate snapshot + WAL truncation

**Mitigation:**
- Batching amortizes fsync cost (100 ops/batch)
- Snapshots triggered when WAL exceeds threshold
- Comprehensive crash recovery tests

### Why Binary WAL Format?

**Rationale:**
1. Compact: no JSON/text parsing overhead
2. Variable length: only store actual key/value bytes
3. Simple to parse: fixed-width length prefixes

**Drawbacks:**
- Not human-readable (vs JSON)
- Need tooling for debugging

**Decision**: Binary chosen for performance, can add debug tool later if needed.

### Why CRC64 for Snapshots?

**Rationale:**
1. Lower collision probability than CRC32 (~4 billion times better)
2. Minimal overhead (8 bytes vs 4 bytes)
3. Standard for large datasets

**Trade-off:** Slightly slower than CRC32, but negligible for our use case.

## Data Flow

### Write Path (with WAL)
```
1. Client: SET key value ttl
2. HashTable: Append to WAL (batched)
3. HashTable: Apply to in-memory hash table
4. WAL: Flush batch → fsync (every 100 ops)
5. Return success to client
```

### Read Path
```
1. Client: GET key
2. HashTable: Lookup in hash table
3. If found and not expired: return value
4. If expired: return null, mark for cleanup
5. Return to client
```

### Recovery Path
```
1. Engine starts
2. If snapshot exists: Load snapshot
3. Replay WAL entries after snapshot
4. Handle partial writes gracefully (incomplete records at EOF)
5. Ready for operations
```

## File Layout

### Header Files (`engine/include/`)
- `hash_table.h` - Core hash table with eviction and TTL
- `lru_policy.h` - LRU eviction policy
- `lfu_policy.h` - LFU eviction policy
- `wal.h` - Write-Ahead Log writer/reader
- `snapshot.h` - Snapshot writer/reader with CRC64
- `sharded_engine.h` - Async shared-nothing engine
- `sharded_engine_sync.h` - Sync fine-grained locking
- `mutex_engine.h` - Global mutex baseline

### Implementation (`engine/src/`)
- Corresponding .cpp files for each header

### Tests (`engine/tests/`)
- `test_hash_table.cpp` (6 tests)
- `test_lru.cpp` (2 tests)
- `test_lfu.cpp` (2 tests)
- `test_ttl.cpp` (4 tests)
- `test_sharded_engine.cpp` (4 tests)
- `test_wal.cpp` (6 tests)
- `test_snapshot.cpp` (5 tests)

**Total: 29 unit tests**

### Benchmarks (`engine/bench/`)
- `bench_single_threaded.cpp` - Baseline ops/sec
- `bench_multi_threaded.cpp` - Scaling with threads
- `bench_comparison.cpp` - Fine-grained vs global mutex
- `bench_persistence.cpp` - WAL overhead and recovery time

## Future Architecture (Phases 4-6)

### Phase 4: Network I/O
- RESP protocol parser (Redis-compatible)
- epoll-based network server
- io_uring-based network server (comparison)
- Choose backend based on measured performance

### Phase 5: Go Integration
- C API wrapper for C++ engine
- cgo bridge in Go
- Operation batching to minimize FFI overhead
- RESP server in Go routing to C++ engine

### Phase 6: Raft Consensus
- Leader election with randomized timeouts
- Log replication to majority before commit
- Followers apply committed entries to local engine
- Linearizable writes through Raft
- Fast-path local reads (may be stale on followers)

### Phase 7 (Stretch): Multi-Raft Sharding
- Consistent hashing for key distribution
- Independent Raft group per shard
- Minimal key redistribution on membership changes

## Testing Strategy

### Unit Tests
- Test each component in isolation
- Cover normal cases and edge cases
- Focus on correctness

### Integration Tests
- WAL replay after crash
- Snapshot + WAL recovery
- Multi-threaded concurrent access

### Benchmarks
- Measure actual performance
- Compare design alternatives (fine-grained vs global mutex)
- Document with hardware specs

### Property-Based Tests (Planned)
- Using RapidCheck or similar
- Test invariants across random inputs
- Catch edge cases unit tests miss

## Performance Characteristics

### Single-Threaded Baseline
- **SET**: 1,149,559 ops/sec
- **GET**: 1,343,331 ops/sec
- **MIXED**: 1,220,705 ops/sec

### Multi-Core Scaling
- **4 threads**: 172,474 ops/sec (fine-grained)
- **8 threads**: 239,195 ops/sec (fine-grained)
- **Global mutex**: ~61K ops/sec (flat, no scaling)

### Memory
- ValueEntry overhead: ~40 bytes (value + metadata)
- Hash table load factor: 0.75 (resize threshold)
- Eviction policies track O(n) metadata

## Build System

**CMake** with the following targets:
- `engine` - Static library with all components
- `test_*` - Individual test executables
- `bench_*` - Benchmark executables

**Compiler**: GCC 13+ or Clang 14+
**Standard**: C++17
**Build Type**: Release (-O3 optimization)

## Deployment (Planned)

### Single Node
```
./kvstore_node --port 6379 --data-dir /var/lib/kvstore
```

### Cluster (3-5 nodes)
```
# Node 1 (seed)
./kvstore_node --port 6379 --raft-port 7000 --data-dir /var/lib/kvstore1

# Node 2
./kvstore_node --port 6380 --raft-port 7001 --data-dir /var/lib/kvstore2 --join localhost:7000

# Node 3
./kvstore_node --port 6381 --raft-port 7002 --data-dir /var/lib/kvstore3 --join localhost:7000
```

## Configuration (Planned)

```yaml
# kvstore.yaml
storage:
  memory_limit: 1GB
  eviction_policy: lru
  
persistence:
  wal_enabled: true
  wal_batch_size: 100
  snapshot_threshold: 100MB
  
network:
  port: 6379
  backend: io_uring  # or epoll
  
cluster:
  enabled: true
  raft_port: 7000
  seed_nodes:
    - node1:7000
    - node2:7001
```

## Monitoring (Planned)

Expose metrics via HTTP endpoint:
- ops/sec (read, write, total)
- latency percentiles (p50, p99, p999)
- memory usage
- eviction rate
- WAL size
- Raft state (leader, term, commit index)

## Summary

This architecture demonstrates:
- **Low-level systems programming**: Custom data structures, memory management, concurrency
- **Distributed systems**: Consensus, replication, failure handling (planned)
- **Performance engineering**: Multi-core scaling, benchmarking, optimization
- **Production readiness**: Persistence, crash recovery, testing

The current implementation (Phase 3) provides a solid foundation with a fully functional storage engine, comprehensive persistence layer, and measured performance characteristics.
