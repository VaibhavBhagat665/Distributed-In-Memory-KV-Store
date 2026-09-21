# Distributed In-Memory KV Store

A high-performance distributed key-value store demonstrating both low-level systems programming and distributed consensus.

## Architecture

- **C++ Storage Engine**: Thread-per-core, shared-nothing architecture with custom memory management
- **Go Consensus Layer**: From-scratch Raft implementation for cluster coordination
- **Integration**: C++ engine embedded in Go process via cgo

## Project Status

🚧 **Under Development** - Following spec-driven development methodology

Current Phase: **Phase 1** - Core storage engine implementation

## Build Requirements

- CMake 3.15+
- C++17 compiler (GCC 9+ / Clang 10+)
- Go 1.21+

## Build Instructions

### C++ Engine

```bash
cd engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Go Node

```bash
cd node
go build ./...
```

## Testing

```bash
# C++ tests
cd engine/build
ctest

# Go tests
cd node
go test ./...
```

## Benchmarks

Performance numbers will be documented in `docs/benchmarks.md` as they are measured with memtier_benchmark.

## Documentation

- [Architecture](docs/architecture.md) - System design and component breakdown
- [Benchmarks](docs/benchmarks.md) - Performance measurements and methodology
- [Requirements](. kiro/specs/distributed-kv-store/requirements.md) - Formal requirements specification
- [Design](. kiro/specs/distributed-kv-store/design.md) - Detailed design document

## License

This is a portfolio project for educational purposes.
