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

**Total Tests: 29**
- Hash table: 6/6 ✓
- LRU: 2/2 ✓
- LFU: 2/2 ✓
- TTL: 4/4 ✓
- Sharded engine: 4/4 ✓
- WAL: 6/6 ✓
- Snapshot: 5/5 ✓

**Benchmarks:**
- All benchmarks should complete without errors
- Actual numbers will vary based on your hardware
- Results will be used to update docs/benchmarks.md

## What to Run Now

Since Phase 3 (persistence) is just completed, run:

```bash
# Full build + Phase 3 tests + persistence benchmark
cd ~/kv-build && \
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* . && \
cd engine && \
rm -rf build && \
cmake -B build -DCMAKE_BUILD_TYPE=Release && \
cmake --build build && \
echo "===== Running WAL Tests =====" && \
./build/bin/test_wal && \
echo "===== Running Snapshot Tests =====" && \
./build/bin/test_snapshot && \
echo "===== Running Persistence Benchmark =====" && \
./build/bin/bench_persistence
```

This will verify Phase 3 is working correctly!


## Phase 6: Raft Consensus Testing

### Build Phase 6 (Raft)

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

### Critical Test: Leader Failover (Phase 6 MVP DoD)

This is the **Definition of Done** test for Phase 6:

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

**Success Criteria:**
- ✅ New leader elected in <1 second
- ✅ No committed data lost
- ✅ Cluster continues serving requests
- ✅ All nodes agree on log contents

### Full Phase 6 Test Suite

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

## Phase 6 Next Steps

To complete Phase 6 MVP:
1. ✅ Core Raft algorithm implemented
2. ✅ RPC layer working
3. ✅ Persistent state
4. ⏳ Integrate apply loop with C++ KV engine
5. ⏳ End-to-end test: client → Raft → KV store
6. ⏳ Measure and document leader failover timing

Once these are done, Phase 6 is **complete** and the distributed KV store MVP is done!
