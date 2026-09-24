# Build and Test Commands for WSL Ubuntu

## Prerequisites
Ensure you have these installed in WSL Ubuntu:
```bash
# Check versions
cmake --version    # Should be 3.15+
g++ --version      # Should be GCC 13+
```

## Full Build and Test Workflow

### Step 1: Create build directory and sync files
```bash
# From WSL Ubuntu terminal
mkdir -p ~/kv-build
cd /mnt/c/Users/DELL/Desktop/projects/kv
cp -r . ~/kv-build/
cd ~/kv-build/engine
```

### Step 2: Clean build
```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Step 3: Run all unit tests
```bash
# Test hash table (6 tests)
./build/bin/test_hash_table

# Test LRU eviction (2 tests)
./build/bin/test_lru

# Test LFU eviction (2 tests)
./build/bin/test_lfu

# Test TTL expiration (4 tests)
./build/bin/test_ttl

# Test sharded engine (4 tests)
./build/bin/test_sharded_engine

# Test WAL (6 tests)
./build/bin/test_wal

# Test snapshots (5 tests)
./build/bin/test_snapshot
```

### Step 4: Run benchmarks
```bash
# Single-threaded baseline
./build/bin/bench_single_threaded

# Multi-threaded scaling (1, 2, 4, 8 threads)
./build/bin/bench_multi_threaded

# Fine-grained vs global mutex comparison
./build/bin/bench_comparison

# Persistence overhead (NEW - Phase 3)
./build/bin/bench_persistence
```

### Step 5: Copy results back to Windows (for git)
```bash
cd ~/kv-build
cp -r . /mnt/c/Users/DELL/Desktop/projects/kv/
```

## Quick Commands

### Just build:
```bash
cd ~/kv-build && cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* . && cd engine && rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

### Build and run all tests:
```bash
cd ~/kv-build && cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* . && cd engine && rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/bin/test_hash_table && ./build/bin/test_lru && ./build/bin/test_lfu && ./build/bin/test_ttl && ./build/bin/test_sharded_engine && ./build/bin/test_wal && ./build/bin/test_snapshot
```

### Build and run Phase 3 tests only:
```bash
cd ~/kv-build && cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* . && cd engine && rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/bin/test_wal && ./build/bin/test_snapshot
```

### Build and run persistence benchmark:
```bash
cd ~/kv-build && cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* . && cd engine && rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/bin/bench_persistence
```

## Troubleshooting

### If build fails with missing directories:
```bash
mkdir -p ~/kv-build
```

### If you get permission errors on /mnt/c:
The build must happen in ~/kv-build (Linux filesystem), not in /mnt/c (Windows filesystem) due to WSL file permission issues.

### If tests fail:
1. Check that all source files were copied: `ls ~/kv-build/engine/src/`
2. Verify clean build: `rm -rf ~/kv-build/engine/build`
3. Check for compilation errors in the cmake output

## Expected Test Results

**Total Tests: 46 (All Passing)**

**C++ Tests (39):**
- Hash table: 6/6 ✓
- LRU: 2/2 ✓
- LFU: 2/2 ✓
- TTL: 4/4 ✓
- Sharded engine: 4/4 ✓
- WAL: 6/6 ✓
- Snapshot: 5/5 ✓
- RESP: 10/10 ✓

**Go Tests (7):**
- Engine integration: 4/4 ✓
- Raft consensus: 3/3 ✓

**Benchmarks:**
- All benchmarks complete successfully
- Results documented in docs/benchmarks.md


## Phase 6: Raft Consensus Testing

### Build and Test Raft (Phase 6 Complete)

```bash
cd ~/kv-build/node

# Run Raft unit tests
go test ./raft -v
```

Expected output:
```
=== RUN   TestLeaderElection
    Leader elected: node1 (term 1)
--- PASS: TestLeaderElection (2.01s)
=== RUN   TestLogReplication
    Leader: node2
    Proposed 'SET a 1' at index 1, term 1
    ...
--- PASS: TestLogReplication (3.15s)
=== RUN   TestBasicConsensus
    Initial leader: node3 (term 1)
    ...
--- PASS: TestBasicConsensus (4.28s)
PASS
```

### Interactive Raft Cluster Demo

```bash
cd ~/kv-build/node

# Build demo
go build -o bin/raft_test ./cmd/raft_test

# Run 5-node cluster
./bin/raft_test
```

Expected output:
```
=== Raft Cluster Test ===

Starting 5-node cluster on ports 9000-9004...
[RPC] Listening on localhost:9000
[RPC] Listening on localhost:9001
...

Waiting for leader election...

✓ Leader elected: node2 (term 1)

=== Testing Log Replication ===

[1] Proposing: SET key1 value1
  ✓ Accepted at index 1, term 1
[node2] Applied command at index 1: SET key1 value1
[node1] Applied command at index 1: SET key1 value1
...

Cluster is running. Press Ctrl+C to stop.
```

### Critical Test: Leader Failover

This test demonstrates Phase 6 completion:

```bash
# Terminal 1: Start cluster
cd ~/kv-build/node
./bin/raft_test

# Wait for leader election, note the leader node

# Terminal 2: Kill leader process
ps aux | grep raft_test
kill -9 <LEADER_PID>

# Terminal 1: Observe failover
# Should see:
# - "Starting election for term 2" 
# - New leader elected within 1 second
# - Cluster continues accepting commands
```

**Success Criteria (All Met):**
- ✅ New leader elected in <1 second
- ✅ No committed data lost
- ✅ Cluster continues serving requests
- ✅ All nodes agree on log contents

### Complete Test Suite

Run all tests to verify the system:

```bash
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/

# Run all tests
cd node
go test ./raft -v

# Run interactive demo
go build -o bin/raft_test ./cmd/raft_test
./bin/raft_test

# Copy back to Windows
cd ~/kv-build
cp -r . /mnt/c/Users/DELL/Desktop/projects/kv/
```

---

## Project Status

**All 6 Phases Complete:**
- ✅ Phase 0: Project structure and build system
- ✅ Phase 1: Core hash table with eviction policies
- ✅ Phase 2: Multi-threaded engine with fine-grained locking
- ✅ Phase 3: Persistence (WAL + Snapshots)
- ✅ Phase 4: Network layer (RESP protocol + epoll)
- ✅ Phase 5: Go integration via cgo
- ✅ Phase 6: Raft consensus and cluster management

**Total: 46/46 tests passing**

This is a production-ready distributed in-memory KV store demonstrating systems programming, distributed consensus, and performance engineering.
