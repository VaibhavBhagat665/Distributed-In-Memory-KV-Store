# Design Document: Distributed In-Memory KV Store

## Overview

This system is a distributed, persistent in-memory key-value store built to demonstrate both low-level performance engineering and distributed systems expertise. The architecture uses a polyglot design: a C++ storage engine optimized for performance-critical operations, embedded via cgo into a Go process that handles consensus and orchestration.

The design deliberately splits responsibilities along the data plane / control plane boundary:
- **C++ StorageEngine**: Thread-per-core, shared-nothing architecture for maximum throughput with custom memory management and I/O handling
- **Go Node**: Raft consensus, cluster membership, client protocol handling, and cgo integration

This mirrors real-world distributed databases (CockroachDB, TiKV) where a fast storage kernel is wrapped by a separate replication layer.

### Why C++ for Storage?

C++ provides direct control over memory layout, cache efficiency, and threading primitives. The performance gains from shared-nothing concurrency, custom allocation, and kernel-bypass I/O are measurable and meaningful for an in-memory system where every nanosecond matters. This is where "I understand systems programming" gets proven with actual throughput-vs-cores graphs.

### Why Go for Consensus?

Raft is fundamentally about concurrent state machines: handling timeouts, processing append-entries RPCs, managing log replication across goroutines. Go's goroutines and channels map naturally onto this problem. Additionally, "distributed systems" in backend hiring circa 2026 means Go fluency for consensus/orchestration layers.

### Why cgo (with IPC fallback)?

cgo allows zero-copy embedding of the C++ engine in-process. The trade-off is cgo call overhead (~tens of nanoseconds per call) and memory management complexity across the FFI boundary. If cgo proves too brittle during implementation, we fall back to Unix domain sockets for IPC (higher latency but cleaner boundaries). Either choice is defensible; the important part is measuring and documenting the trade-off.

## Architecture

```mermaid
graph TB
    subgraph "Node Process (Go)"
        Client[RESP Client Protocol]
        Raft[Raft State Machine]
        CGO[cgo Bridge]
    end
    
    subgraph "Storage Engine (C++)"
        Router[Request Router]
        T1[Thread 1<br/>Shard 0-N/4]
        T2[Thread 2<br/>Shard N/4-N/2]
        T3[Thread 3<br/>Shard N/2-3N/4]
        T4[Thread 4<br/>Shard 3N/4-N]
        WAL[Write-Ahead Log]
        Snap[Snapshot Manager]
        Net[Network I/O<br/>epoll / io_uring]
    end
    
    Client -->|Write Cmd| Raft
    Client -->|Read Cmd| CGO
    Raft -->|Committed Entry| CGO
    CGO -->|Apply Op| Router
    Router -->|Hash(key)| T1
    Router -->|Hash(key)| T2
    Router -->|Hash(key)| T3
    Router -->|Hash(key)| T4
    T1 & T2 & T3 & T4 --> WAL
    T1 & T2 & T3 & T4 --> Snap
    Net -.->|Network I/O| Client
    
    subgraph "Cluster (Multi-Node)"
        N1[Node 1<br/>Leader]
        N2[Node 2<br/>Follower]
        N3[Node 3<br/>Follower]
    end
    
    N1 -->|AppendEntries| N2
    N1 -->|AppendEntries| N3
    N2 -->|RequestVote| N1
```

### Data Flow

**Write Path (Linearizable):**
1. Client sends SET command via RESP to any node
2. If node is not Raft leader, forward to leader
3. Leader proposes entry to Raft log
4. Leader replicates entry to majority of followers (AppendEntries RPCs)
5. Once replicated to majority, leader commits entry
6. Leader applies committed entry to local StorageEngine via cgo
7. Followers apply committed entries to their local StorageEngines
8. Leader returns success to client

**Read Path (Fast, potentially stale):**
1. Client sends GET command via RESP to any node
2. Node immediately reads from local StorageEngine via cgo (no Raft coordination)
3. Node returns value to client
4. Note: Follower reads may observe slightly stale data until a future Raft heartbeat commits newer writes

**Persistence Flow:**
1. All applied operations append to WAL with fsync before acknowledgment
2. Periodic snapshots serialize entire in-memory state
3. On snapshot completion, WAL is truncated
4. On restart: load snapshot (if exists) + replay WAL tail

## Components and Interfaces

### C++ StorageEngine

**Core Components:**

1. **HashTable** (custom implementation)
   - Collision strategy: chaining vs. open addressing (TBD: research trade-offs, likely chaining for simplicity + iterator stability)
   - Load factor threshold: 0.75 → trigger resize
   - Key: `std::string_view` (avoid copy), Value: pointer to `ValueEntry`
   - Thread-safe? No — each thread owns its shard

2. **ValueEntry** (heap-allocated via slab allocator)
   ```cpp
   struct ValueEntry {
       std::string value;
       uint64_t access_count;    // for LFU
       uint64_t last_access_ns;  // for LRU
       uint64_t expiry_ns;       // for TTL (0 = no expiry)
   };
   ```

3. **SlabAllocator** (custom memory manager)
   - Size classes: 64B, 128B, 256B, 512B, 1KB, 2KB, 4KB, ...
   - Each class maintains a freelist of reusable slabs
   - Falls back to `malloc` for sizes > max slab (512KB)
   - Reduces fragmentation and malloc overhead for steady-state workload

4. **EvictionPolicy** (abstract interface, two implementations)
   - `LRUPolicy`: maintain doubly-linked list ordered by `last_access_ns`
   - `LFUPolicy`: maintain min-heap ordered by `access_count`
   - On memory pressure: evict according to active policy

5. **TTLManager**
   - Lazy expiry: check `expiry_ns` on GET, delete if expired
   - Active expiry: background thread (1 per StorageEngine) scans random sample every 100ms, deletes expired keys
   - Trade-off: scan frequency vs. memory waste from un-accessed expired keys

6. **WALWriter** (per-thread instance)
   - Append-only file: `wal_shard_{id}.log`
   - Format: `[op_type:1][key_len:4][key:var][value_len:4][value:var]`
   - Fsync after each batch (configurable batch size, default 100 ops)
   - Thread-local → no locking required

7. **SnapshotManager** (shared across threads, writer-locks during snapshot)
   - Format: `snapshot_{timestamp}.dat`
   - Serialization: iterate all shards, write `[count:8][entries...]`
   - Non-blocking strategy: COW (copy-on-write) or double-buffering? TBD based on implementation complexity

8. **NetworkServer** (two implementations: epoll and io_uring)
   - `EpollServer`: event loop on `epoll_wait`, edge-triggered mode
   - `IoUringServer`: submission/completion queue with batched syscalls
   - Both expose same interface: `void HandleConnection(int fd)`
   - RESP parsing: state machine for multi-bulk arrays

9. **RequestRouter**
   - Hash key → shard ID: `shard = murmur3_hash(key) % num_threads`
   - Lock-free dispatch: each thread polls a per-thread queue (SPSC queue from Boost.Lockfree)

### Go Node

**Core Components:**

1. **RESPServer** (package `node/server`)
   - Listens on TCP port (default 6379)
   - Spawns goroutine per client connection
   - Parses RESP commands: `GET`, `SET`, `DELETE`, `EXPIRE`
   - Routes writes → Raft, reads → local engine

2. **RaftNode** (package `node/raft`, from scratch)
   - **State**: Follower | Candidate | Leader
   - **Persistent state** (on disk): `currentTerm`, `votedFor`, `log[]`
   - **Volatile state**: `commitIndex`, `lastApplied`
   - **Leader-only state**: `nextIndex[]`, `matchIndex[]` per follower
   - **Election timeout**: random in [150ms, 300ms]
   - **Heartbeat interval**: 50ms (< min election timeout)
   
   Key methods:
   ```go
   type RaftNode interface {
       Propose(cmd []byte) error          // Submit write to Raft
       ApplyCommitted() <-chan LogEntry    // Stream of committed entries to apply
       GetLeader() (nodeID, term)
   }
   ```

3. **CGOBridge** (package `node/engine`)
   - Wraps C++ StorageEngine with Go-callable functions
   - Manages memory across boundary: Go allocates request buffer, C++ writes response, Go frees
   - Batching strategy: accumulate up to N operations, single cgo call
   
   ```go
   /*
   #cgo CXXFLAGS: -std=c++17 -O3
   #cgo LDFLAGS: -L./build -lengine
   #include "engine.h"
   */
   import "C"
   
   func (b *CGOBridge) Set(key, value string, ttl int64) error
   func (b *CGOBridge) Get(key string) (string, error)
   func (b *CGOBridge) Delete(key string) error
   ```

4. **ClusterManager** (package `node/cluster`)
   - Maintains peer connections (gRPC or raw TCP)
   - Sends `AppendEntries`, `RequestVote` RPCs
   - Handles node join/leave (Raft joint consensus for membership changes)

### Interfaces and Contracts

**StorageEngine C API** (exposed to Go via cgo):
```c
// Opaque handle
typedef struct EngineHandle EngineHandle;

// Lifecycle
EngineHandle* engine_create(int num_threads, size_t mem_limit_bytes);
void engine_destroy(EngineHandle* handle);

// Operations
int engine_set(EngineHandle* h, const char* key, size_t klen, 
               const char* val, size_t vlen, int64_t ttl_ns);
int engine_get(EngineHandle* h, const char* key, size_t klen,
               char* out_buf, size_t* out_len);
int engine_delete(EngineHandle* h, const char* key, size_t klen);

// Persistence
int engine_replay_wal(EngineHandle* h, const char* wal_dir);
int engine_snapshot(EngineHandle* h, const char* snapshot_path);
```

**Raft RPC Messages** (Go protobuf or hand-rolled):
```go
type AppendEntriesRequest struct {
    Term         uint64
    LeaderID     string
    PrevLogIndex uint64
    PrevLogTerm  uint64
    Entries      []LogEntry
    LeaderCommit uint64
}

type RequestVoteRequest struct {
    Term         uint64
    CandidateID  string
    LastLogIndex uint64
    LastLogTerm  uint64
}
```

## Data Models

### In-Memory Key-Value Entry (C++)
```cpp
struct KeyEntry {
    std::string key;              // Owned by hash table
    ValueEntry* value;            // Pointer to slab-allocated ValueEntry
    uint64_t hash;                // Cached hash for fast lookup/resize
};

struct ValueEntry {
    std::string value;            // Actual data (max 512KB)
    uint64_t access_count;        // Incremented on each GET (for LFU)
    uint64_t last_access_ns;      // Updated on each GET (for LRU)
    uint64_t expiry_ns;           // Nanoseconds since epoch; 0 = no expiry
};
```

### Raft Log Entry (Go)
```go
type LogEntry struct {
    Term    uint64
    Index   uint64
    Command []byte  // Serialized operation: [op_type][key][value]
}
```

### WAL Record (Binary Format, C++)
```
[OpType:1 byte]  // 0=SET, 1=DELETE, 2=EXPIRE
[KeyLen:4 bytes (LE)]
[Key:KeyLen bytes]
[ValueLen:4 bytes (LE)] // 0 for DELETE
[Value:ValueLen bytes]
[TTL:8 bytes (LE)]      // 0 for no expiry
```

### Snapshot Format (Binary, C++)
```
[Magic:4 bytes "KVSS"]
[Version:4 bytes]
[NumEntries:8 bytes]
For each entry:
  [KeyLen:4][Key:var][ValueLen:4][Value:var][AccessCount:8][LastAccessNs:8][ExpiryNs:8]
[Checksum:8 bytes CRC64]
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Storage Engine Properties

**Property 1: SET-GET round trip**
*For any* valid key-value pair, storing it with SET and then retrieving it with GET should return the exact same value.
**Validates: Requirements 1.1, 1.2**

**Property 2: DELETE removes keys**
*For any* key that exists in the store, performing DELETE followed by GET should return null, indicating the key no longer exists.
**Validates: Requirements 1.4**

**Property 3: Non-existent keys return null**
*For any* key that has never been stored or has been deleted, GET should return a null response.
**Validates: Requirements 1.3**

**Property 4: Hash collisions preserve data**
*For any* set of keys that hash to the same bucket (forced collisions), all keys should remain independently accessible with correct values after insertion.
**Validates: Requirements 2.2**

**Property 5: Access metadata updates on read**
*For any* key, performing GET should update the access metadata (access_count and last_access_ns) such that repeated GETs show monotonically increasing values.
**Validates: Requirements 3.4**

**Property 6: Expired keys return null**
*For any* key stored with a TTL, attempting to GET the key after the TTL has elapsed should return null (lazy expiry).
**Validates: Requirements 4.1, 4.2**

**Property 7: Deterministic shard routing**
*For any* key, computing its shard assignment should be deterministic—the same key always routes to the same thread ID regardless of how many times the computation is performed.
**Validates: Requirements 5.2**

### Persistence Properties

**Property 8: WAL recovery restores state**
*For any* sequence of write operations (SET, DELETE, EXPIRE), after performing the operations, killing the process, and restarting with WAL replay, all committed operations should be reflected in the recovered state.
**Validates: Requirements 7.1, 7.3**

**Property 9: Snapshot + WAL recovery restores complete state**
*For any* state S1 captured in a snapshot, followed by additional writes W, restarting and loading the snapshot + replaying subsequent WAL entries should produce state S1 + W.
**Validates: Requirements 8.1, 8.4**

**Property 10: Crash recovery preserves data integrity**
*For any* in-progress write operation, killing the process with SIGKILL and restarting should result in either (a) the write is fully present, or (b) the write is fully absent, but never partial/corrupted data.
**Validates: Requirements 16.1**

### Protocol Properties

**Property 11: RESP command execution round trip**
*For any* valid RESP-formatted command, sending it to the Node and receiving a response should result in a valid RESP-formatted response and the correct operation applied to storage.
**Validates: Requirements 10.1, 10.2**

**Property 12: RESP error handling**
*For any* invalid or malformed RESP command, the Node should return a RESP-formatted error message without crashing or corrupting state.
**Validates: Requirements 10.3**

**Property 13: Pipelined request ordering**
*For any* sequence of pipelined RESP commands sent without waiting for responses, all responses should arrive in the same order as the requests were sent.
**Validates: Requirements 10.4**

### Raft Consensus Properties

**Property 14: Single leader per term**
*For any* Raft term, across all nodes in the cluster, at most one node should be in Leader state for that term.
**Validates: Requirements 11.1, 11.2**

**Property 15: Majority quorum for commits**
*For any* write command proposed by the leader, the log entry should only be committed (and applied to storage) after being replicated to a majority of nodes.
**Validates: Requirements 11.3**

**Property 16: State machine consistency**
*For any* sequence of committed log entries, all nodes should apply them in the same order and reach identical storage states (comparing snapshots across nodes produces identical data).
**Validates: Requirements 11.4**

**Property 17: Log reconciliation on rejoin**
*For any* node that was partitioned and has divergent log entries, when it reconnects to the cluster, its log should converge to match the leader's log through overwriting inconsistent entries.
**Validates: Requirements 11.7**

**Property 18: Write replication after commit**
*For any* write command (SET, DELETE, EXPIRE) sent to the cluster, once the client receives success, reading that key from any node (after allowing for replication delay) should reflect the committed write.
**Validates: Requirements 12.3, 12.4**

**Property 19: Leader crash preserves committed writes**
*For any* set of writes that received successful acknowledgment (committed), killing the current leader and allowing a new leader election should result in all those committed writes being present in the new leader's state.
**Validates: Requirements 16.2**

**Property 20: Follower catch-up after restart**
*For any* follower node that crashes, if the cluster continues processing writes, restarting the follower should result in it replaying missed log entries and converging to the current committed state.
**Validates: Requirements 16.4**

**Property 21: Consistent membership view**
*For any* stable cluster (no ongoing membership changes), querying all reachable nodes for their view of cluster membership should return identical member lists.
**Validates: Requirements 15.2**

### Multi-Raft Sharding Properties (Stretch)

**Property 22: Consistent shard assignment**
*For any* key and consistent hash ring configuration, the key should always map to the same shard (RaftGroup) regardless of how many times the hash is computed.
**Validates: Requirements 18.2**

**Property 23: Independent shard replication**
*For any* two different shards, a leader election or write operation in shard A should not block or interfere with operations in shard B.
**Validates: Requirements 18.5**

## Error Handling

### Client Errors
- **Invalid commands**: Return RESP error, maintain state
- **Oversized values (> 512KB)**: Return error, reject write
- **Malformed RESP**: Return error, close connection if unrecoverable parse state

### Storage Engine Errors
- **Memory limit reached**: Trigger eviction per active policy; if eviction fails (e.g., all keys have recent access), return out-of-memory error to client
- **Hash table resize failure**: Log critical error, return temporary error to client (system remains operational with degraded performance)
- **Slab allocation failure**: Fall back to malloc; if malloc fails, return out-of-memory error

### Persistence Errors
- **WAL write failure (disk full)**: Return error to client, do NOT commit write in memory (maintain durability invariant)
- **WAL fsync failure**: Return error to client, log critical error, optionally stop accepting writes (configurable fail-stop mode)
- **Snapshot failure**: Log error, continue operation (next snapshot attempt may succeed; WAL keeps growing but system remains available)
- **Replay corruption (CRC mismatch)**: Halt startup, require manual intervention (corrupted WAL is data loss and cannot be auto-recovered)

### Raft/Consensus Errors
- **Split brain prevention**: Nodes never commit entries without majority quorum; partitioned minority becomes read-only
- **Log divergence**: Follower log conflicts are resolved by AppendEntries consistency check (prevLogIndex/prevLogTerm), overwriting divergent entries
- **Leader unavailability**: Writes return timeout error if no leader elected within client timeout (typically 5-10 seconds)
- **RPC failure**: Retry with exponential backoff; if node unreachable beyond timeout, mark as down and exclude from quorum

### Network Errors
- **Connection failures**: Close socket, clean up client state, log error
- **Partial reads/writes**: Buffer and retry or close connection if timeout exceeded
- **Backpressure (too many connections)**: Reject new connections with error, or queue with bounded queue length

## Testing Strategy

### Unit Testing

**C++ Storage Engine:**
- Hash table: collision handling, resizing, load factor tracking
- Eviction policies: LRU ordering, LFU heap correctness
- TTL: lazy expiry on access, active expiry background scan
- Slab allocator: correct size class selection, freelist management
- WAL writer: correct serialization, fsync behavior
- Snapshot: serialization/deserialization, CRC validation
- RESP parser: state machine correctness for simple and nested arrays

**Go Node:**
- Raft state machine: follower → candidate → leader transitions
- Log replication: AppendEntries correctness, log matching
- Leader election: RequestVote correctness, term advancement
- cgo bridge: memory ownership, batching logic

### Property-Based Testing

Property-based tests will be implemented using:
- **C++ engine**: RapidCheck (https://github.com/emil-e/rapidcheck) - a QuickCheck port for C++
- **Go node**: gopter (https://github.com/leanovate/gopter) - property-based testing for Go

Each property test will:
- Run a minimum of 100 iterations with randomly generated inputs
- Be tagged with a comment linking to the design document property number
- Use smart generators that constrain inputs to valid domains (e.g., keys are non-empty strings, TTL values are positive)

**Property test generators:**
- `genKey()`: non-empty strings, alphanumeric + special chars, 1-256 bytes
- `genValue()`: byte arrays, 0-512KB, with stratified sampling (small/medium/large)
- `genTTL()`: 0 (no expiry) or positive nanoseconds up to 1 hour
- `genCommand()`: SET/GET/DELETE with random keys/values
- `genRaftLog()`: sequences of log entries with varying terms and indices
- `genClusterState()`: node states (follower/candidate/leader), terms, log positions

### Integration Testing

**Single-node end-to-end:**
- Client → RESP server → cgo bridge → storage engine → WAL
- Run memtier_benchmark with small dataset, verify correctness
- Kill process mid-benchmark, restart, verify recovery

**Multi-node cluster:**
- 3-node cluster: leader election, write replication, follower reads
- 5-node cluster: majority quorum with 2 nodes down
- Partition scenarios (using iptables or Docker network manipulation):
  - Isolate leader → verify new election
  - Isolate minority → verify majority continues
  - Heal partition → verify log reconciliation

### Stress Testing

- **Load**: memtier_benchmark with sustained load (1M ops/sec target) for 1 hour
- **Churn**: continuous node restarts during load
- **Partition**: rolling network partitions during load

### Benchmark Testing

All benchmarks use memtier_benchmark with:
- Hardware: document CPU model, core count, RAM, disk type (NVMe SSD)
- Workload: 50% SET / 50% GET, 128-byte values, zipfian key distribution
- Clients: 4 threads, 50 connections each (200 total)

**Benchmark suite:**
1. **Single-threaded baseline**: StorageEngine with 1 thread, measure ops/sec
2. **Multi-core scaling**: 1, 2, 4, 8 threads, plot throughput vs. core count
3. **Global-mutex comparison**: same workload with naive global-lock design, measure scaling loss
4. **epoll vs. io_uring**: both network backends under same load, measure p50/p99 latency and throughput
5. **Persistence overhead**: WAL on vs. off, measure throughput difference
6. **Recovery time**: dataset sizes (100MB, 1GB, 10GB), measure restart time
7. **Cluster write latency**: 3-node cluster, measure p50/p99/p999 write latency
8. **Failover time**: kill leader with SIGKILL, measure time until new leader elected and first write succeeds

Each benchmark result is recorded in `docs/benchmarks.md` with:
- Date of run
- Commit SHA
- Hardware specs
- Exact memtier command line
- Raw output (throughput, latency percentiles)
- Graph (if multi-dimensional)

