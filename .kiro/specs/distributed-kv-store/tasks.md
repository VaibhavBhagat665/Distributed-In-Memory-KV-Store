# Implementation Plan

- [ ] 1. Set up project structure and build system
  - Create directory structure: `/engine` (C++), `/node` (Go), `/bench`, `/docs`
  - Set up CMake build for C++ engine with C++17, optimization flags, and test targets
  - Set up Go module with appropriate dependencies
  - Create initial README.md and docs/architecture.md stubs
  - Configure git to ignore build artifacts and IDE files
  - _Requirements: All (foundation)_

- [ ] 2. Implement core C++ hash table with collision handling
  - Implement custom hash table with chaining for collision resolution
  - Support insert, lookup, delete operations with O(1) average case
  - Implement dynamic resizing when load factor exceeds 0.75
  - Track statistics: load factor, collision rate, bucket distribution
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.2, 2.3_

- [ ] 2.1 Write property test for hash table round-trip
  - **Property 1: SET-GET round trip**
  - **Validates: Requirements 1.1, 1.2**

- [ ] 2.2 Write property test for DELETE operation
  - **Property 2: DELETE removes keys**
  - **Validates: Requirements 1.4**

- [ ] 2.3 Write property test for non-existent keys
  - **Property 3: Non-existent keys return null**
  - **Validates: Requirements 1.3**

- [ ] 2.4 Write property test for hash collision handling
  - **Property 4: Hash collisions preserve data**
  - **Validates: Requirements 2.2**

- [ ] 3. Implement LRU eviction policy
  - Create ValueEntry struct with access metadata (last_access_ns, access_count, expiry_ns)
  - Implement LRU using doubly-linked list ordered by last access time
  - Update access timestamp on every GET operation
  - Evict least-recently-used entry when memory limit is reached
  - _Requirements: 3.1, 3.4_

- [ ] 3.1 Write unit test for LRU eviction behavior
  - Test specific eviction order with controlled access patterns
  - _Requirements: 3.1_

- [ ] 3.2 Write property test for access metadata updates
  - **Property 5: Access metadata updates on read**
  - **Validates: Requirements 3.4**

- [ ] 4. Implement LFU eviction policy
  - Implement LFU using min-heap ordered by access count
  - Increment access counter on every GET operation
  - Evict least-frequently-used entry when memory limit is reached
  - Make eviction policy configurable (LRU vs LFU) at engine creation
  - _Requirements: 3.2, 3.4_

- [ ] 4.1 Write unit test for LFU eviction behavior
  - Test specific eviction order with controlled access frequencies
  - _Requirements: 3.2_

- [ ] 5. Implement TTL expiration mechanism
  - Store expiry timestamp (expiry_ns) in ValueEntry
  - Implement lazy expiry: check TTL on GET, return null and mark for deletion if expired
  - Implement active expiry: background thread scans random samples every 100ms
  - Make scan frequency configurable
  - _Requirements: 4.1, 4.2, 4.3_

- [ ] 5.1 Write property test for TTL expiration
  - **Property 6: Expired keys return null**
  - **Validates: Requirements 4.1, 4.2**

- [ ] 5.2 Write unit test for active expiry scan
  - Test that background scan removes expired keys
  - _Requirements: 4.3_

- [ ] 6. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 7. Implement thread-per-core shared-nothing architecture
  - Partition keyspace across N worker threads (N configurable)
  - Implement deterministic key-to-shard routing: `shard_id = murmur3_hash(key) % N`
  - Create per-thread hash table instances (no shared state)
  - Implement lock-free request queue per thread (SPSC queue)
  - Create request router that dispatches operations to owning thread
  - _Requirements: 5.1, 5.2_

- [ ] 7.1 Write property test for deterministic routing
  - **Property 7: Deterministic shard routing**
  - **Validates: Requirements 5.2**

- [ ] 8. Implement custom slab allocator
  - Create slab allocator with power-of-two size classes: 64B, 128B, 256B, 512B, 1KB, 2KB, 4KB, up to 512KB
  - Maintain freelist per size class for memory reuse
  - Fall back to malloc for sizes exceeding 512KB
  - Integrate with ValueEntry allocation
  - Track allocation statistics: slab utilization, fragmentation metrics
  - _Requirements: 6.1, 6.2, 6.3_

- [ ] 8.1 Write unit tests for slab allocator
  - Test size class selection, freelist reuse, and malloc fallback
  - _Requirements: 6.1, 6.2, 6.3_

- [ ] 9. Implement baseline single-threaded benchmark
  - Create simple benchmark harness that measures ops/sec for SET and GET operations
  - Run with single thread (baseline for multi-core comparison)
  - Document ops/sec in docs/benchmarks.md with [TODO: run benchmark] placeholder
  - _Requirements: 17.2_

- [ ] 10. Implement multi-threaded benchmark and global-mutex comparison
  - Create global-mutex baseline version of storage engine for comparison
  - Benchmark both implementations with 1, 2, 4, 8 threads
  - Plot throughput vs. core count for both designs
  - Document scaling comparison in docs/benchmarks.md
  - _Requirements: 5.4, 17.3_

- [ ] 11. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 12. Implement Write-Ahead Log (WAL)
  - Define WAL binary format: `[op_type:1][key_len:4][key:var][value_len:4][value:var][ttl:8]`
  - Implement per-thread WAL writer: append-only file `wal_shard_{id}.log`
  - Call fsync after each batch (configurable batch size, default 100 ops)
  - Append operation to WAL before updating in-memory hash table
  - _Requirements: 7.1, 7.2_

- [ ] 13. Implement WAL replay for recovery
  - Implement WAL reader that parses binary format
  - On engine initialization, replay all WAL files to reconstruct in-memory state
  - Handle partial writes at end of WAL (incomplete records)
  - Verify operations are applied in correct order
  - _Requirements: 7.3_

- [ ] 13.1 Write property test for WAL recovery
  - **Property 8: WAL recovery restores state**
  - **Validates: Requirements 7.1, 7.3**

- [ ] 14. Implement snapshot mechanism
  - Define snapshot binary format with magic header and CRC64 checksum
  - Implement snapshot writer that serializes complete in-memory state
  - Use copy-on-write or double-buffering to avoid blocking operations during snapshot
  - Trigger snapshot when WAL exceeds configurable size threshold
  - Truncate WAL after successful snapshot creation
  - _Requirements: 8.1, 8.3, 7.4_

- [ ] 15. Implement snapshot-based recovery
  - On engine initialization, check for existing snapshot file
  - If snapshot exists, load it first, then replay subsequent WAL entries
  - Validate snapshot CRC64 checksum before loading
  - _Requirements: 8.4_

- [ ] 15.1 Write property test for snapshot + WAL recovery
  - **Property 9: Snapshot + WAL recovery restores complete state**
  - **Validates: Requirements 8.1, 8.4**

- [ ] 15.2 Write property test for crash recovery integrity
  - **Property 10: Crash recovery preserves data integrity**
  - **Validates: Requirements 16.1**

- [ ] 15.3 Write unit test for WAL truncation after snapshot
  - Test that WAL is truncated after successful snapshot
  - _Requirements: 8.3_

- [ ] 16. Benchmark persistence overhead and recovery time
  - Measure throughput with WAL enabled vs. disabled
  - Measure recovery time for datasets: 100MB, 1GB, 10GB
  - Document results in docs/benchmarks.md
  - _Requirements: 17.6_

- [ ] 17. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 18. Implement RESP protocol parser
  - Create state machine for parsing RESP simple strings, errors, integers, bulk strings, and arrays
  - Handle pipelined requests (multiple commands without waiting for responses)
  - Implement RESP response formatting
  - Support core commands: GET, SET, DELETE, EXPIRE
  - _Requirements: 10.1, 10.2, 10.3, 10.4_

- [ ] 18.1 Write property test for RESP round-trip
  - **Property 11: RESP command execution round trip**
  - **Validates: Requirements 10.1, 10.2**

- [ ] 18.2 Write property test for RESP error handling
  - **Property 12: RESP error handling**
  - **Validates: Requirements 10.3**

- [ ] 18.3 Write property test for pipelined requests
  - **Property 13: Pipelined request ordering**
  - **Validates: Requirements 10.4**

- [ ] 19. Implement epoll-based network server
  - Create TCP listener on configurable port
  - Use epoll for asynchronous I/O with edge-triggered mode
  - Handle connection accept, read, write, and close events
  - Integrate RESP parser with epoll event loop
  - Wire up parsed commands to storage engine operations
  - _Requirements: 9.1_

- [ ] 20. Implement io_uring-based network server
  - Create alternative network server implementation using io_uring
  - Use submission/completion queue model for batched syscalls
  - Expose same interface as epoll server for drop-in replacement
  - Make backend selectable via configuration flag
  - _Requirements: 9.2, 9.3_

- [ ] 21. Benchmark epoll vs io_uring with memtier_benchmark
  - Install and configure memtier_benchmark
  - Run load tests with both backends: measure throughput, p50/p99 latency
  - Test with varying connection counts (10, 50, 100, 200 connections)
  - Document comparison in docs/benchmarks.md with evidence-based backend choice
  - _Requirements: 9.4, 9.5, 17.4_

- [ ] 22. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 23. Create C API for cgo integration
  - Define C header with opaque EngineHandle type
  - Implement C wrapper functions: `engine_create`, `engine_destroy`, `engine_set`, `engine_get`, `engine_delete`
  - Implement `engine_replay_wal` and `engine_snapshot` for persistence
  - Handle memory ownership: caller allocates output buffers, C++ writes into them
  - _Requirements: 14.1_

- [ ] 24. Set up Go module and project structure
  - Initialize Go module in `/node` directory
  - Create package structure: `node/server` (RESP), `node/raft` (consensus), `node/engine` (cgo bridge)
  - Set up cgo build tags and linker flags to link against C++ engine
  - _Requirements: 14.1_

- [ ] 25. Implement cgo bridge in Go
  - Create Go wrappers for C API functions using cgo
  - Manage memory across cgo boundary: allocate C buffers, convert to Go strings, free C memory
  - Implement operation batching to minimize cgo call overhead (batch up to 100 ops per call)
  - Document cgo call overhead measurement
  - _Requirements: 14.2, 14.3_

- [ ] 26. Implement RESP server in Go
  - Create TCP listener in Go (default port 6379)
  - Spawn goroutine per client connection
  - Parse RESP commands and route to storage engine or Raft based on operation type
  - Wire up reads (GET) to direct cgo calls
  - Wire up writes (SET, DELETE, EXPIRE) to Raft proposal (stub for now)
  - _Requirements: 10.1, 10.2_

- [ ] 26.1 Write integration test for Go RESP server with C++ engine
  - Test end-to-end: client → RESP server → cgo → engine → response
  - _Requirements: 10.1, 10.2_

- [ ] 27. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 28. Implement Raft persistent state
  - Define Raft state structures: `currentTerm`, `votedFor`, `log[]` with `LogEntry{Term, Index, Command}`
  - Implement persistence to disk using JSON or binary format
  - Load Raft state on node startup
  - _Requirements: 11.7_

- [ ] 29. Implement Raft leader election
  - Implement three states: Follower, Candidate, Leader with state transitions
  - Implement RequestVote RPC: send/receive, vote granting logic, term comparison
  - Implement election timeout with randomization (150-300ms range)
  - Implement Candidate → Leader transition when majority votes received
  - Reset to Follower when higher term discovered
  - _Requirements: 11.1, 11.2_

- [ ] 29.1 Write property test for single leader per term
  - **Property 14: Single leader per term**
  - **Validates: Requirements 11.1, 11.2**

- [ ] 30. Implement Raft log replication
  - Implement AppendEntries RPC: send/receive, log matching check (prevLogIndex/prevLogTerm)
  - Leader sends heartbeats (empty AppendEntries) every 50ms
  - Leader tracks `nextIndex[]` and `matchIndex[]` per follower
  - Advance `commitIndex` when entry replicated to majority
  - Followers apply committed entries to their state machine
  - _Requirements: 11.3, 11.4_

- [ ] 30.1 Write property test for majority quorum
  - **Property 15: Majority quorum for commits**
  - **Validates: Requirements 11.3**

- [ ] 30.2 Write property test for state machine consistency
  - **Property 16: State machine consistency**
  - **Validates: Requirements 11.4**

- [ ] 31. Implement log reconciliation and conflict resolution
  - Implement log consistency check in AppendEntries handler
  - When log conflict detected, follower truncates conflicting entries and overwrites with leader's log
  - Leader decrements `nextIndex` on AppendEntries rejection and retries
  - _Requirements: 11.5_

- [ ] 31.1 Write unit test for log reconciliation
  - Test divergent log scenario and verify convergence
  - _Requirements: 11.5_

- [ ] 31.2 Write property test for log reconciliation on rejoin
  - **Property 17: Log reconciliation on rejoin**
  - **Validates: Requirements 11.7**

- [ ] 32. Integrate Raft with storage engine apply path
  - Implement state machine apply function that calls cgo bridge to apply committed commands
  - Create apply loop that reads from Raft commit channel and applies to storage engine
  - Ensure operations are applied in log order
  - _Requirements: 12.3_

- [ ] 33. Implement write path through Raft
  - When RESP server receives write command (SET, DELETE, EXPIRE), propose to Raft
  - If node is not leader, forward request to current leader
  - Once entry is committed, return success to client
  - Implement timeout handling for write failures
  - _Requirements: 12.1, 12.4_

- [ ] 33.1 Write property test for write replication
  - **Property 18: Write replication after commit**
  - **Validates: Requirements 12.3, 12.4**

- [ ] 33.2 Write unit test for write timeout handling
  - Test that client receives error when write fails to achieve quorum within timeout
  - _Requirements: 12.5_

- [ ] 34. Implement cluster membership tracking
  - Implement cluster configuration: list of peer nodes with addresses
  - Implement node discovery via seed nodes on startup
  - Expose cluster status API: current leader, term, member list
  - _Requirements: 15.1, 15.4_

- [ ] 34.1 Write property test for consistent membership view
  - **Property 21: Consistent membership view**
  - **Validates: Requirements 15.2**

- [ ] 34.2 Write unit test for cluster join
  - Test node startup and cluster join process
  - _Requirements: 15.1_

- [ ] 35. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 36. Implement end-to-end cluster testing
  - Create test harness for starting 3-node and 5-node clusters
  - Test write replication across cluster
  - Test leader election after leader crash (SIGKILL)
  - Test follower reads return data (potentially stale)
  - _Requirements: 11.6, 16.2, 16.3_

- [ ] 36.1 Write property test for leader crash recovery
  - **Property 19: Leader crash preserves committed writes**
  - **Validates: Requirements 16.2**

- [ ] 36.2 Write property test for follower catch-up
  - **Property 20: Follower catch-up after restart**
  - **Validates: Requirements 16.4**

- [ ] 36.3 Write unit test for partition tolerance
  - Test that minority partition becomes unavailable while majority continues
  - _Requirements: 15.3_

- [ ] 37. Benchmark cluster performance
  - Measure write latency (p50/p99/p999) with memtier_benchmark against 3-node cluster
  - Measure leader failover time: kill leader → new leader elected → first write succeeds
  - Document results in docs/benchmarks.md
  - _Requirements: 17.7_

- [ ] 38. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Stretch Goals (Optional - Multi-Raft Sharding)

- [ ] 39. Implement consistent hashing for shard assignment
  - Implement hash ring with configurable virtual nodes per physical node
  - Implement key-to-shard mapping: hash key onto ring, find responsible shard
  - Support K independent shards (K configurable)
  - _Requirements: 18.1_

- [ ] 39.1 Write property test for consistent shard assignment
  - **Property 22: Consistent shard assignment**
  - **Validates: Requirements 18.2**

- [ ] 40. Implement multi-Raft group architecture
  - Each shard runs independent Raft instance with own leader election and log
  - Route operations to correct RaftGroup based on key hash
  - Ensure isolation: operations in different shards don't block each other
  - _Requirements: 18.5_

- [ ] 40.1 Write property test for independent shard replication
  - **Property 23: Independent shard replication**
  - **Validates: Requirements 18.5**

- [ ] 41. Implement key redistribution on membership change
  - When node joins/leaves, compute affected key ranges
  - Migrate keys between shards as needed
  - Measure percentage of keys moved vs. total keyspace
  - Compare against naive modulo hashing
  - _Requirements: 18.3, 18.4_

- [ ] 41.1 Write unit test for limited key redistribution
  - Test that only affected keys are moved on membership change
  - _Requirements: 18.3_

## Documentation and Packaging

- [ ] 42. Finalize architecture documentation
  - Complete docs/architecture.md with updated diagrams and implementation details
  - Document cgo integration approach and trade-offs
  - Document chosen network backend (epoll or io_uring) with justification
  - _Requirements: All_

- [ ] 43. Finalize benchmark documentation
  - Complete docs/benchmarks.md with all measured results
  - Replace all [TODO: run benchmark] placeholders with actual numbers
  - Include hardware specifications, memtier configurations, and raw outputs
  - Add performance comparison graphs
  - _Requirements: 17.1-17.7_

- [ ] 44. Create README with quick start guide
  - Add project overview and architecture summary
  - Add build instructions for both C++ engine and Go node
  - Add instructions for running single-node and cluster setups
  - Add example client commands
  - Link to architecture and benchmark docs
  - _Requirements: All_

- [ ] 45. Prepare resume bullets and interview Q&A
  - Draft 3-5 resume bullets with actual measured numbers from benchmarks
  - Create Q&A document with 8-10 "defend this project" questions and answers
  - Cover: why polyglot, shared-nothing vs global lock, cgo overhead, backend choice, Raft safety, failure scenarios
  - _Requirements: All_

## Notes
- All property-based tests and unit tests are required for comprehensive correctness validation
- Checkpoint tasks ensure incremental progress and early bug detection
- All benchmark placeholders must be replaced with real measured values before project completion
- Multi-Raft sharding (tasks 39-41) is a stretch goal - prioritize MVP completion (tasks 1-38) first
