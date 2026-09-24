# Requirements Document

## Introduction

This document specifies the requirements for a distributed in-memory key-value store designed as a portfolio-defining project for backend/infrastructure engineering roles. The system demonstrates both low-level performance engineering through a custom C++ storage engine and distributed systems expertise through a from-scratch Raft consensus implementation in Go. The architecture deliberately uses a polyglot design to showcase distinct, high-value skill sets: a performance-critical C++ data plane and a Go-based consensus/orchestration control plane.

## Glossary

- **KVStore**: The complete distributed key-value storage system comprising multiple nodes
- **StorageEngine**: The C++ component responsible for in-memory data storage, indexing, eviction, and persistence
- **Node**: A single instance of the distributed system, implemented as a Go process embedding the StorageEngine via cgo
- **RaftGroup**: A collection of nodes running the Raft consensus protocol to replicate data
- **RESP**: Redis Serialization Protocol, the wire protocol for client-server communication
- **Shard**: A partition of the keyspace owned by a specific thread or RaftGroup
- **WAL**: Write-Ahead Log, an append-only persistence mechanism
- **LRU**: Least Recently Used eviction policy
- **LFU**: Least Frequently Used eviction policy
- **TTL**: Time To Live, expiration time for keys
- **cgo**: Go's foreign function interface for calling C/C++ code
- **Shared-nothing**: An architecture where each thread owns exclusive data with no cross-thread locking on the hot path
- **memtier_benchmark**: A Redis-compatible load testing tool used for performance measurement

## Requirements

### Requirement 1: Core Storage Operations

**User Story:** As a client application, I want to perform basic key-value operations (SET, GET, DELETE) on the store, so that I can use it as a primary data cache.

#### Acceptance Criteria

1. WHEN a client sends a SET command with a key and value THEN the StorageEngine SHALL store the key-value pair and return success
2. WHEN a client sends a GET command for an existing key THEN the StorageEngine SHALL return the associated value
3. WHEN a client sends a GET command for a non-existent key THEN the StorageEngine SHALL return a null response
4. WHEN a client sends a DELETE command for an existing key THEN the StorageEngine SHALL remove the key-value pair and return success
5. THE StorageEngine SHALL support values up to 512KB in size

### Requirement 2: Custom Hash Table Implementation

**User Story:** As a system architect, I want a custom-built hash table as the core indexing structure, so that I can demonstrate understanding of fundamental data structure trade-offs and performance characteristics.

#### Acceptance Criteria

1. THE StorageEngine SHALL implement a hash table with collision resolution strategy documented and justified
2. WHEN hash collisions occur THEN the StorageEngine SHALL handle them without data loss or corruption
3. WHEN the hash table load factor exceeds a defined threshold THEN the StorageEngine SHALL resize the table and rehash existing entries
4. THE StorageEngine SHALL expose hash table statistics including load factor, collision rate, and bucket distribution

### Requirement 3: Memory Eviction Policies

**User Story:** As a system operator, I want configurable eviction policies (LRU and LFU), so that I can tune memory usage based on my workload characteristics.

#### Acceptance Criteria

1. WHEN the StorageEngine reaches its configured memory limit and LRU policy is active THEN the StorageEngine SHALL evict the least recently accessed key
2. WHEN the StorageEngine reaches its configured memory limit and LFU policy is active THEN the StorageEngine SHALL evict the least frequently accessed key
3. THE StorageEngine SHALL track access metadata required for both LRU and LFU policies with minimal overhead
4. WHEN a client retrieves a key THEN the StorageEngine SHALL update the access metadata for eviction policy tracking

### Requirement 4: Time-Based Key Expiration

**User Story:** As a client application, I want to set expiration times on keys, so that temporary data is automatically cleaned up without manual intervention.

#### Acceptance Criteria

1. WHEN a client sends a SET command with a TTL parameter THEN the StorageEngine SHALL store the expiration timestamp with the key
2. WHEN a client attempts to GET an expired key THEN the StorageEngine SHALL return a null response and mark the key for deletion (lazy expiry)
3. THE StorageEngine SHALL periodically scan for and remove expired keys (active expiry) with configurable scan frequency
4. WHEN an expired key is accessed or scanned THEN the StorageEngine SHALL reclaim its memory

### Requirement 5: Thread-Per-Core Shared-Nothing Architecture

**User Story:** As a performance engineer, I want a shared-nothing, thread-per-core architecture, so that the system achieves near-linear scaling across CPU cores without lock contention.

#### Acceptance Criteria

1. THE StorageEngine SHALL partition the keyspace across N worker threads where N is configurable
2. WHEN a request arrives for a key THEN the StorageEngine SHALL route it to exactly one owning thread based on key hash
3. WHILE processing requests within a single shard THEN the StorageEngine SHALL require zero cross-thread locks on the hot path
4. THE StorageEngine SHALL achieve measurable throughput improvement over a global-mutex baseline design when utilizing multiple cores

### Requirement 6: Custom Memory Allocation

**User Story:** As a performance engineer, I want a custom slab allocator for value storage, so that I can reduce malloc overhead and memory fragmentation under high load.

#### Acceptance Criteria

1. THE StorageEngine SHALL implement a slab allocator with power-of-two size classes for value storage
2. WHEN allocating memory for a value THEN the StorageEngine SHALL use the slab allocator rather than system malloc for sizes within configured thresholds
3. WHEN deallocating a value THEN the StorageEngine SHALL return the memory to the appropriate slab for reuse
4. THE StorageEngine SHALL expose memory allocation statistics including slab utilization and fragmentation metrics

### Requirement 7: Write-Ahead Log Persistence

**User Story:** As a system operator, I want write-ahead logging with fsync, so that committed data survives process crashes without corruption.

#### Acceptance Criteria

1. WHEN a write operation is committed THEN the StorageEngine SHALL append the operation to the WAL before acknowledging success
2. WHEN appending to the WAL THEN the StorageEngine SHALL call fsync to ensure durability on the storage device
3. WHEN the Node process starts THEN the StorageEngine SHALL replay the WAL to reconstruct in-memory state
4. WHEN the WAL exceeds a configured size threshold THEN the StorageEngine SHALL trigger snapshot creation and log truncation

### Requirement 8: Snapshot-Based Recovery

**User Story:** As a system operator, I want periodic snapshotting, so that recovery time remains bounded as the dataset grows.

#### Acceptance Criteria

1. WHEN triggered THEN the StorageEngine SHALL serialize the complete in-memory state to a snapshot file
2. WHEN creating a snapshot THEN the StorageEngine SHALL do so without blocking ongoing operations
3. WHEN a snapshot completes successfully THEN the StorageEngine SHALL truncate the WAL to remove operations already captured in the snapshot
4. WHEN the Node process starts and a snapshot exists THEN the StorageEngine SHALL load the snapshot and replay only subsequent WAL entries
5. THE StorageEngine SHALL complete recovery of a documented dataset size within a measured and documented time bound

### Requirement 9: Network I/O Backend Selection

**User Story:** As a performance engineer, I want both epoll and io_uring network backends implemented and benchmarked, so that I can choose the optimal I/O mechanism based on measured evidence rather than assumptions.

#### Acceptance Criteria

1. THE StorageEngine SHALL implement a network server using epoll for asynchronous I/O
2. THE StorageEngine SHALL implement a network server using io_uring for asynchronous I/O
3. THE StorageEngine SHALL expose both implementations behind a common interface allowing runtime selection
4. WHEN benchmarked under realistic load THEN the StorageEngine SHALL produce documented throughput and latency measurements comparing both backends
5. THE StorageEngine SHALL use the backend that demonstrates superior performance under the documented workload

### Requirement 10: RESP Protocol Compatibility

**User Story:** As a client developer, I want the system to speak the RESP protocol, so that I can use existing Redis client libraries without modification.

#### Acceptance Criteria

1. THE Node SHALL accept client connections and parse commands according to the RESP protocol specification
2. WHEN a valid RESP command is received THEN the Node SHALL execute the corresponding operation and return a RESP-formatted response
3. WHEN an invalid RESP command is received THEN the Node SHALL return a RESP error message
4. THE Node SHALL support pipelined requests where multiple commands are sent without waiting for responses

### Requirement 11: Raft Consensus Implementation

**User Story:** As a distributed systems engineer, I want a from-scratch Raft implementation for replication, so that the cluster maintains consistency and availability despite node failures.

#### Acceptance Criteria

1. THE RaftGroup SHALL implement leader election with randomized timeouts to prevent split elections
2. WHEN a leader is elected THEN the RaftGroup SHALL ensure only one leader exists per term
3. WHEN a write command is received by the leader THEN the RaftGroup SHALL replicate the log entry to a majority of followers before committing
4. WHEN a log entry is committed THEN all nodes SHALL apply it to their local StorageEngine in the same order
5. WHEN a follower's log diverges from the leader THEN the RaftGroup SHALL resolve the conflict by overwriting the follower's inconsistent entries
6. WHEN a leader fails THEN the RaftGroup SHALL elect a new leader within a measured time bound
7. WHEN a node restarts THEN the RaftGroup SHALL restore its Raft state and catch up to the current log position

### Requirement 12: Linearizable Writes via Raft

**User Story:** As a client application requiring strong consistency, I want all write operations to go through Raft consensus, so that I observe linearizable behavior across the cluster.

#### Acceptance Criteria

1. WHEN a client sends a write command (SET, DELETE, EXPIRE) to any Node THEN the Node SHALL forward the command to the Raft leader if it is not the leader
2. WHEN the Raft leader receives a write command THEN the Node SHALL propose it to the Raft log before applying it to the local StorageEngine
3. WHEN a log entry is committed by Raft consensus THEN the Node SHALL apply the write to the local StorageEngine
4. WHEN a write is successfully replicated to a majority THEN the Node SHALL return success to the client
5. IF a write fails to achieve majority replication within a timeout THEN the Node SHALL return an error to the client

### Requirement 13: Fast-Path Local Reads

**User Story:** As a latency-sensitive client application, I want read operations to execute directly against the local storage engine, so that I achieve minimal read latency.

#### Acceptance Criteria

1. WHEN a client sends a GET command to any Node THEN the Node SHALL execute the read directly against its local StorageEngine without Raft coordination
2. THE Node SHALL document that follower reads may return slightly stale data
3. THE Node SHALL support read operations with throughput limited only by local StorageEngine performance

### Requirement 14: cgo Integration Layer

**User Story:** As a system integrator, I want a well-designed cgo boundary between Go and C++, so that the integration is performant and maintainable.

#### Acceptance Criteria

1. THE Node SHALL embed the StorageEngine via cgo function calls
2. WHEN crossing the cgo boundary THEN the Node SHALL batch operations where possible to minimize call overhead
3. WHEN passing data across the cgo boundary THEN the Node SHALL manage memory ownership clearly to prevent leaks or use-after-free bugs
4. THE Node SHALL document the cgo call overhead and its impact on operation latency

### Requirement 15: Cluster Membership and Health

**User Story:** As a cluster operator, I want nodes to track cluster membership, so that I can safely add or remove nodes from the cluster.

#### Acceptance Criteria

1. WHEN a Node starts THEN the Node SHALL join the cluster by contacting seed nodes configured at startup
2. THE RaftGroup SHALL maintain a consistent view of cluster membership across all nodes
3. WHEN a node becomes unreachable THEN the RaftGroup SHALL detect the failure and continue operating if a majority remains
4. THE Node SHALL expose cluster status including current leader, term, and member health

### Requirement 16: Crash Recovery and Correctness

**User Story:** As a system operator, I want the system to recover correctly from crashes, so that I can trust it with production data.

#### Acceptance Criteria

1. WHEN a Node process is killed with SIGKILL during a write operation THEN the Node SHALL recover on restart without data corruption
2. WHEN the Raft leader crashes THEN the RaftGroup SHALL elect a new leader without losing any committed writes
3. WHEN the Raft leader crashes THEN the RaftGroup SHALL complete leader election within a measured and documented time bound
4. WHEN a follower crashes and restarts THEN the Node SHALL catch up to the current committed log position by replaying missed entries

### Requirement 17: Performance Measurement Infrastructure

**User Story:** As a project maintainer, I want comprehensive benchmark infrastructure using industry-standard tools, so that all published performance numbers are reproducible and credible.

#### Acceptance Criteria

1. THE KVStore SHALL be tested using memtier_benchmark with documented workload configurations
2. WHEN benchmarking single-threaded performance THEN the project SHALL document operations per second for a baseline workload
3. WHEN benchmarking multi-core scaling THEN the project SHALL document throughput versus core count and compare against a global-mutex baseline
4. WHEN benchmarking network backends THEN the project SHALL document throughput and latency for both epoll and io_uring under documented connection counts
5. WHEN benchmarking cluster failover THEN the project SHALL document failover time from leader crash to new leader election
6. WHEN benchmarking recovery THEN the project SHALL document recovery time for a documented dataset size
7. THE KVStore SHALL record all benchmark results with hardware specifications in a dedicated benchmarks document

### Requirement 18: Multi-Raft Sharding (Stretch Goal)

**User Story:** As a scalability engineer, I want the keyspace sharded across independent Raft groups, so that the system scales horizontally beyond a single replication group's write throughput limit.

#### Acceptance Criteria

1. THE KVStore SHALL partition the keyspace into K shards using consistent hashing with virtual nodes
2. WHEN a key is accessed THEN the Node SHALL route the operation to the owning shard's RaftGroup
3. WHEN a node joins or leaves the cluster THEN the KVStore SHALL redistribute only the affected portion of keys rather than rehashing all keys
4. THE KVStore SHALL measure and document the percentage of keys redistributed on membership changes compared to naive modulo hashing
5. EACH RaftGroup SHALL replicate its shard independently with its own leader election and log replication

## Requirements Coverage Notes

All acceptance criteria are structured using EARS (Easy Approach to Requirements Syntax) patterns. Each criterion specifies the system component responsible (StorageEngine, Node, RaftGroup, KVStore) and uses active voice with measurable conditions. Performance-related criteria explicitly require measurement and documentation of results with placeholders for specific numbers that must be filled in through actual benchmark runs.
