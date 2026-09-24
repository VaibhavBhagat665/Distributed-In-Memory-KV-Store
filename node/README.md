# KV Store Go Node

Go layer for the distributed KV store, embedding the C++ engine via cgo.

## Architecture

```
┌─────────────────────────────────────┐
│         Go Process                  │
│  ┌──────────────────────────────┐   │
│  │   RESP Server (TCP)          │   │
│  └──────────┬───────────────────┘   │
│             │                        │
│  ┌──────────▼───────────────────┐   │
│  │   Engine Package (cgo)       │   │
│  └──────────┬───────────────────┘   │
│             │ FFI                    │
│  ═══════════╪═══════════════════════ │
│             │                        │
│  ┌──────────▼───────────────────┐   │
│  │   C++ Storage Engine         │   │
│  │   (libengine.a)              │   │
│  └──────────────────────────────┘   │
└─────────────────────────────────────┘
```

## Building

### Prerequisites

1. **C++ engine must be built first:**
   ```bash
   cd ../engine
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

2. **Go 1.21+** installed

### Build Go code

```bash
cd node

# Build the server binary
go build -o bin/kvserver ./cmd/server

# Run tests
go test ./engine -v
go test ./server -v
```

## Running

### Start the server

```bash
./bin/kvserver -addr :6379 -threads 4 -mem 1073741824
```

Options:
- `-addr`: Server address (default: `:6379`)
- `-threads`: Number of engine threads (default: `4`)
- `-mem`: Memory limit in bytes (default: `1GB`)

### Test with redis-cli

```bash
redis-cli -p 6379

> PING
PONG

> SET mykey "Hello, World!"
OK

> GET mykey
"Hello, World!"

> SET expiring "temporary" EX 10
OK

> DEL mykey
(integer) 1
```

## cgo Configuration

The `engine.go` file contains cgo directives:

```go
/*
#cgo CXXFLAGS: -std=c++17 -I../../engine/include
#cgo LDFLAGS: -L../../engine/build/lib -lengine -lstdc++ -lpthread
*/
```

These tell cgo:
- Where to find C++ headers (`-I../../engine/include`)
- Where to find the compiled library (`-L../../engine/build/lib`)
- Which libraries to link (`-lengine`, `-lstdc++`, `-lpthread`)

## Project Structure

```
node/
├── cmd/
│   └── server/          # Main server binary
│       └── main.go
├── engine/              # cgo bindings to C++ engine
│   ├── engine.go
│   └── engine_test.go
├── server/              # RESP protocol server
│   └── resp_server.go
├── go.mod
└── README.md
```

## Testing

### Unit tests

```bash
# Test cgo bindings
go test ./engine -v

# Test RESP server (TODO)
go test ./server -v
```

### Integration test with C++ engine

```bash
# Build and test everything
cd ../engine && cmake --build build && cd ../node
go test ./... -v
```

## Development

### Hot reload during development

```bash
# Use air or similar for hot reload
go install github.com/cosmtrek/air@latest
air
```

### Debugging cgo

```bash
# Enable cgo debug output
CGO_CFLAGS="-g -O0" go build -x ./cmd/server

# Run with gdb
gdb ./bin/kvserver
```

## Phase 5 Status

- [x] cgo bindings to C++ engine
- [x] Basic RESP server in Go
- [x] SET/GET/DEL commands
- [x] TTL support
- [ ] Connection pooling
- [ ] Metrics/monitoring
- [ ] Raft integration (Phase 6)

## Next Steps

Phase 6 will add:
1. Raft consensus implementation
2. Multi-node cluster support
3. Leader election and log replication
4. Client request forwarding
