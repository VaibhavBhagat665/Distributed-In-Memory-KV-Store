# Building the Go Node with cgo

## Prerequisites

### 1. C++ Engine Built

The C++ engine must be built first to create `libengine.a`:

```bash
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/
cd engine
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This creates:
- `engine/build/lib/libengine.a` - Static library
- `engine/build/bin/test_*` - Test executables

### 2. Go Installed

Check Go version:
```bash
go version  # Should be 1.21+
```

Install Go on Ubuntu/WSL if needed:
```bash
wget https://go.dev/dl/go1.21.6.linux-amd64.tar.gz
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.21.6.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
```

## Building

### Option 1: Using Make (Recommended)

```bash
cd ~/kv-build
make all          # Build C++ and Go
make test         # Run all tests
make run          # Start server
```

### Option 2: Manual Build

```bash
# 1. Build C++ engine (if not done)
cd ~/kv-build/engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Build Go server
cd ~/kv-build/node
go build -o bin/kvserver ./cmd/server

# 3. Run tests
go test ./engine -v
```

## Testing the cgo Bridge

### Test 1: Go unit tests

```bash
cd ~/kv-build/node
go test ./engine -v
```

Expected output:
```
=== RUN   TestEngineBasicOperations
--- PASS: TestEngineBasicOperations (0.00s)
=== RUN   TestEngineNotFound
--- PASS: TestEngineNotFound (0.00s)
=== RUN   TestEngineTTL
--- PASS: TestEngineTTL (0.15s)
=== RUN   TestEngineMultipleKeys
--- PASS: TestEngineMultipleKeys (0.00s)
PASS
ok      github.com/yourusername/kvstore/engine  0.158s
```

### Test 2: Start server

```bash
cd ~/kv-build/node
./bin/kvserver -addr :6379 -threads 4
```

Expected output:
```
Starting KV store server...
  Address: :6379
  Threads: 4
  Memory limit: 1024 MB
RESP server listening on :6379
```

### Test 3: Connect with redis-cli

In another terminal:
```bash
redis-cli -p 6379

> PING
PONG

> SET hello "world"
OK

> GET hello
"world"

> SET temp "expires" EX 5
OK

> GET temp
"expires"

# Wait 6 seconds
> GET temp
(nil)

> DEL hello
(integer) 1
```

## Troubleshooting

### Error: "cannot find -lengine"

The C++ library wasn't found. Ensure:
1. C++ engine is built: `ls engine/build/lib/libengine.a`
2. Path in `engine.go` cgo directives matches your build location

### Error: "undefined reference to ..."

Missing C++ symbols. Verify:
1. All `.cpp` files are in CMakeLists.txt
2. C++17 standard is set
3. Library is linked with `-lstdc++`

### Error: "cgo: C compiler not found"

Install build tools:
```bash
sudo apt-get update
sudo apt-get install build-essential
```

### cgo Debug Output

```bash
CGO_CFLAGS="-I$(pwd)/engine/include" \
CGO_LDFLAGS="-L$(pwd)/engine/build/lib -lengine -lstdc++ -lpthread" \
go build -x ./cmd/server
```

## Phase 6: Testing Raft Consensus

### Test 1: Raft unit tests

```bash
cd ~/kv-build/node
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

### Test 2: Interactive Raft cluster demo

```bash
cd ~/kv-build/node
CGO_CFLAGS="-I$(pwd)/../engine/include" \
CGO_LDFLAGS="-L$(pwd)/../engine/build/lib -lengine -lstdc++ -lpthread" \
go build -o bin/raft_test ./cmd/raft_test

./bin/raft_test
```

Expected output:
```
=== Raft Cluster Test ===

Starting 5-node cluster on ports 9000-9004...

Waiting for leader election...

✓ Leader elected: node2 (term 1)

=== Testing Log Replication ===

[1] Proposing: SET key1 value1
  ✓ Accepted at index 1, term 1

[2] Proposing: SET key2 value2
  ✓ Accepted at index 2, term 1
...

Cluster is running. Press Ctrl+C to stop.
```

### Test 3: Leader failover test (Phase 6 DoD)

This is the **critical test** for Phase 6 completion:

```bash
# Terminal 1: Start cluster
cd ~/kv-build/node
./bin/raft_test

# Wait for leader election, note the leader

# Terminal 2: Kill leader with kill -9
ps aux | grep raft_test
kill -9 <LEADER_PID>

# Terminal 1: Watch for new leader election
# Should see: "Starting election for term 2"
# Then: "Became leader for term 2"
```

**Success criteria:**
- New leader elected within 1 second
- No committed data lost
- Cluster continues accepting commands

## Copy Back to Windows

After successful build:
```bash
cd ~/kv-build
cp -r . /mnt/c/Users/DELL/Desktop/projects/kv/
```

## Performance Notes

The cgo boundary has some overhead:
- ~50-100ns per call (function call + data marshaling)
- String copies when passing data to/from C++
- For high throughput, batch operations when possible

Our design minimizes overhead by:
- Keeping hot path (GET/SET) in C++
- No per-request allocations in Go
- Direct buffer passing where possible
