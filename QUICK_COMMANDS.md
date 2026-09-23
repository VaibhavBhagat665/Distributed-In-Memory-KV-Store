# Quick Commands Reference

Copy-paste commands for building and testing the distributed KV store.

## Phase 6: Test Raft Consensus

### Run Raft Unit Tests

```bash
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/
cd node
go test ./raft -v
```

### Build and Run Interactive Demo

```bash
cd ~/kv-build/node
go build -o bin/raft_test ./cmd/raft_test
./bin/raft_test
```

### Full Phase 1-6 Build and Test

```bash
# Copy to WSL
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/

# Build C++ engine
cd engine
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run all C++ tests (39 tests)
./build/bin/test_hash_table && \
./build/bin/test_lru && \
./build/bin/test_lfu && \
./build/bin/test_ttl && \
./build/bin/test_sharded_engine && \
./build/bin/test_wal && \
./build/bin/test_snapshot && \
./build/bin/test_resp

# Test Go cgo bridge (4 tests)
cd ../node
CGO_CFLAGS="-I$(pwd)/../engine/include" \
CGO_LDFLAGS="-L$(pwd)/../engine/build/lib -lengine -lstdc++ -lpthread" \
go test ./engine -v

# Test Raft (3 tests)
go test ./raft -v

# Copy back to Windows
cd ~/kv-build
cp -r . /mnt/c/Users/DELL/Desktop/projects/kv/
```

### Leader Failover Test (Phase 6 DoD)

```bash
# Terminal 1: Start cluster
cd ~/kv-build/node
./bin/raft_test

# Terminal 2: Kill leader
ps aux | grep raft_test
kill -9 <PID_OF_LEADER>

# Watch Terminal 1 for new election
```

## Quick Rebuild (After Code Changes)

### Just C++ Engine

```bash
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/
cd engine
cmake --build build
```

### Just Go Tests

```bash
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/
cd node
go test ./raft -v
```

### Everything

```bash
cd ~/kv-build && \
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/ && \
cd engine && \
cmake -B build -DCMAKE_BUILD_TYPE=Release && \
cmake --build build && \
cd ../node && \
go test ./raft -v
```

## Benchmarks

### C++ Benchmarks

```bash
cd ~/kv-build/engine
./build/bin/bench_single_threaded
./build/bin/bench_multi_threaded
./build/bin/bench_comparison
./build/bin/bench_persistence
```

## Git Workflow

### Commit Phase 6 Work

```bash
cd /mnt/c/Users/DELL/Desktop/projects/kv

git status
git add node/raft/
git add node/cmd/raft_test/
git add node/BUILD.md
git add BUILD_AND_TEST.md
git add AGENTS.md
git add PHASE6_STATUS.md
git add QUICK_COMMANDS.md

git commit -m "feat(raft): implement Raft consensus from scratch

- Core Raft algorithm: leader election, log replication, commit protocol
- HTTP-based RPC layer for inter-node communication
- Persistent storage: MemoryLogStore + FileStateStore
- Cluster management utilities
- Unit tests: leader election, log replication, consensus
- Interactive demo: 5-node cluster with command proposals
- Documentation: comprehensive Raft README

Phase 6 infrastructure complete. Next: integrate with KV store."
```

## Environment Setup (If Needed)

### Install Go (Ubuntu/WSL)

```bash
wget https://go.dev/dl/go1.21.6.linux-amd64.tar.gz
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.21.6.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
go version
```

### Install Build Tools

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake
cmake --version
g++ --version
```

## Debugging

### Check Raft Logs

```bash
# The demo prints logs to stdout
./bin/raft_test | tee raft.log
```

### Test Individual RPC

```bash
# Start cluster in one terminal
./bin/raft_test

# In another terminal, test RPC endpoint
curl -X POST http://localhost:9000/raft/request_vote \
  -H "Content-Type: application/json" \
  -d '{"term":1,"candidateId":"test","lastLogIndex":0,"lastLogTerm":0}'
```

### Check Process Status

```bash
# See all raft_test processes
ps aux | grep raft_test

# Count how many nodes running
ps aux | grep raft_test | wc -l

# Kill all (if needed)
pkill -9 raft_test
```

## Next Steps After Testing

Once Phase 6 tests pass:

1. **Integrate with KV store**
   ```bash
   # Create node/server/raft_server.go
   # Wire apply loop to C++ engine
   ```

2. **End-to-end test**
   ```bash
   # Start 5-node cluster
   # Client writes through Raft
   # Verify replication
   ```

3. **Measure failover timing**
   ```bash
   # Automated kill -9 test
   # Record election time
   ```

4. **Mark Phase 6 complete** ✅
