.PHONY: all clean test build-engine build-go run

# Default target
all: build-engine build-go

# Build C++ engine
build-engine:
	@echo "Building C++ engine..."
	cd engine && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# Build Go server
build-go:
	@echo "Building Go server..."
	cd node && go build -o bin/kvserver ./cmd/server

# Run all tests
test: test-engine test-go

# Test C++ engine
test-engine:
	@echo "Testing C++ engine..."
	cd engine/build/bin && \
		./test_hash_table && \
		./test_lru && \
		./test_lfu && \
		./test_ttl && \
		./test_sharded_engine && \
		./test_wal && \
		./test_snapshot && \
		./test_resp

# Test Go code
test-go:
	@echo "Testing Go code..."
	cd node && go test ./... -v

# Run server
run: build-go
	cd node && ./bin/kvserver -addr :6379 -threads 4

# Clean build artifacts
clean:
	rm -rf engine/build
	rm -rf node/bin
	cd node && go clean

# Build for production (optimized)
release: CXXFLAGS += -O3 -march=native
release: build-engine build-go

# Development build (with debug symbols)
dev: CXXFLAGS += -g -O0
dev: build-engine build-go
