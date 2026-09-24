# Development Guide

This document contains development notes, build commands, and troubleshooting tips.

---

## Development Environment

**OS:** Ubuntu 24.04 LTS (WSL2 on Windows)  
**Compiler:** GCC 13.x  
**Build System:** CMake 3.27+  
**Go Version:** 1.21+

---

## Build Commands

### Full Build (C++ + Go)

```bash
# Sync to WSL workspace
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/

# Build C++ engine
cd engine
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build Go binaries
cd ../node
CGO_CFLAGS="-I$(pwd)/../engine/include" \
CGO_LDFLAGS="-L$(pwd)/../engine/build/lib -lengine -lstdc++ -lpthread" \
go build -o bin/kvserver ./cmd/simple_server

# Copy back to Windows
cd ~/kv-build
cp -r . /mnt/c/Users/DELL/Desktop/projects/kv/
```

### Quick Rebuild (After C++ Changes)

```bash
cd ~/kv-build/engine
cmake --build build
```

### Quick Rebuild (After Go Changes)

```bash
cd ~/kv-build/node
CGO_CFLAGS="-I$(pwd)/../engine/include" \
CGO_LDFLAGS="-L$(pwd)/../engine/build/lib -lengine -lstdc++ -lpthread" \
go build -o bin/kvserver ./cmd/simple_server
```

---

## Running Tests

### All C++ Tests

```bash
cd ~/kv-build/engine
./build/bin/test_hash_table && \
./build/bin/test_lru && \
./build/bin/test_lfu && \
./build/bin/test_ttl && \
./build/bin/test_sharded_engine && \
./build/bin/test_wal && \
./build/bin/test_snapshot && \
./build/bin/test_resp
```

### All Go Tests

```bash
cd ~/kv-build/node
CGO_CFLAGS="-I$(pwd)/../engine/include" \
CGO_LDFLAGS="-L$(pwd)/../engine/build/lib -lengine -lstdc++ -lpthread" \
go test ./engine ./raft -v
```

### Single Test

```bash
# C++ specific test
./build/bin/test_hash_table

# Go specific test  
go test ./raft -run TestLeaderElection -v
```

---

## Running Benchmarks

```bash
cd ~/kv-build/engine

# Single-threaded baseline
./build/bin/bench_single_threaded

# Multi-threaded scaling
./build/bin/bench_multi_threaded

# Compare shared-nothing vs global mutex
./build/bin/bench_comparison

# Persistence overhead
./build/bin/bench_persistence
```

---

## Development Workflow

### 1. Making Changes

```bash
# Edit files in Windows (VSCode)
# Files are at: C:\Users\DELL\Desktop\projects\kv\

# Sync to WSL for building
cd ~/kv-build
cp -r /mnt/c/Users/DELL/Desktop/projects/kv/* ~/kv-build/

# Build + test
cd engine && cmake --build build
./build/bin/test_hash_table

# If tests pass, copy back to Windows
cd ~/kv-build
cp -r . /mnt/c/Users/DELL/Desktop/projects/kv/
```

### 2. Git Workflow

```bash
cd /mnt/c/Users/DELL/Desktop/projects/kv

git add .
git commit -m "feat: your change description"
git push origin main
```

---

## Troubleshooting

### C++ Build Issues

**Error: `cmake: command not found`**
```bash
sudo apt-get update
sudo apt-get install cmake build-essential
```

**Error: `undefined reference to std::...`**
- Ensure C++17 is set in CMakeLists.txt
- Add `-lstdc++` to linker flags

### Go Build Issues

**Error: `cannot find -lengine`**
```bash
# Verify library exists
ls ~/kv-build/engine/build/lib/libengine.a

# Rebuild C++ if missing
cd ~/kv-build/engine
cmake --build build
```

**Error: `cgo: C compiler not found`**
```bash
sudo apt-get install build-essential
```

### Runtime Issues

**Server won't start**
- Check if port 6379 is already in use: `lsof -i :6379`
- Kill existing process: `pkill -9 kvserver`

**Tests failing randomly**
- May be timing-related (Raft elections)
- Run tests multiple times to confirm
- Check system load: `top`

---

## Code Style

### C++

```cpp
// Use snake_case for functions and variables
void hash_table_insert(const std::string& key, const std::string& value);

// Use PascalCase for classes
class HashTable {
    // Private members with m_ prefix
    std::unordered_map<std::string, std::string> m_data;
};

// Always use explicit types
auto result = get_value(); // Good
```

### Go

```go
// Use camelCase for unexported
func getLeader() *Node { ... }

// Use PascalCase for exported
func NewNode(config Config) *Node { ... }

// Error handling
if err != nil {
    return fmt.Errorf("operation failed: %w", err)
}
```

---

## Debugging

### C++ with GDB

```bash
# Build with debug symbols
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run under GDB
gdb ./build/bin/test_hash_table
(gdb) break hash_table.cpp:42
(gdb) run
(gdb) print key
```

### Go with Delve

```bash
# Install delve
go install github.com/go-delve/delve/cmd/dlv@latest

# Debug
dlv test ./raft -- -test.run TestLeaderElection
(dlv) break node.go:123
(dlv) continue
```

### Memory Leaks (Valgrind)

```bash
valgrind --leak-check=full ./build/bin/test_hash_table
```

---

## Performance Profiling

### C++ with perf

```bash
# Build with symbols
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Profile
perf record ./build/bin/bench_single_threaded
perf report
```

### Go with pprof

```go
import _ "net/http/pprof"

// In main()
go func() {
    http.ListenAndServe("localhost:6060", nil)
}()
```

Then visit: http://localhost:6060/debug/pprof

---

## CI/CD Notes

Currently manual. Future: GitHub Actions for:
- Automated testing on push
- Benchmark regression detection
- Multi-platform builds (Linux, macOS)

---

## Release Process

1. Run full test suite
2. Update version in CMakeLists.txt and go.mod
3. Tag release: `git tag v1.0.0`
4. Push tag: `git push origin v1.0.0`
5. Create GitHub release with binaries

---

## Useful Commands

```bash
# Find TODO comments
grep -r "TODO" engine/src node/

# Count lines of code
cloc engine/src engine/include node/

# Check for memory leaks
valgrind --leak-check=full ./build/bin/kvserver

# Monitor server
watch -n 1 'echo "PING" | nc localhost 6379'

# Stress test
for i in {1..1000}; do echo "SET key$i value$i" | nc localhost 6379; done
```

---

## Resources

- [CMake Documentation](https://cmake.org/documentation/)
- [cgo Documentation](https://pkg.go.dev/cmd/cgo)
- [Raft Paper](https://raft.github.io/raft.pdf)
- [RESP Protocol Spec](https://redis.io/docs/reference/protocol-spec/)

---

Last Updated: 2026-09-24
