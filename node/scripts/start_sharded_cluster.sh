#!/bin/bash

# Start Sharded KV Cluster (3 shards × 3 nodes = 9 Raft nodes)

echo "=== Starting Sharded KV Cluster ==="
echo ""

# Kill any existing processes
pkill -f sharded_server

# Build
echo "Building sharded server..."
go build -o bin/sharded_server ./cmd/sharded_server
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "Starting 3 nodes (each running 3 shards)..."
echo ""

# Node 0 (runs one replica of each shard)
echo "Starting Node 0 on port 7000..."
./bin/sharded_server --node-id=0 --port=7000 --shards=3 > logs/node0.log 2>&1 &
sleep 0.5

# Node 1
echo "Starting Node 1 on port 7001..."
./bin/sharded_server --node-id=1 --port=7001 --shards=3 > logs/node1.log 2>&1 &
sleep 0.5

# Node 2
echo "Starting Node 2 on port 7002..."
./bin/sharded_server --node-id=2 --port=7002 --shards=3 > logs/node2.log 2>&1 &

# Wait for initialization
echo ""
echo "Waiting for shards to initialize..."
sleep 3

echo ""
echo "✓ Sharded cluster started!"
echo ""
echo "Cluster topology:"
echo "  - 3 shards (0, 1, 2)"
echo "  - 3 replicas per shard"
echo "  - 9 Raft nodes total"
echo ""
echo "Client ports:"
echo "  - Node 0: localhost:7000"
echo "  - Node 1: localhost:7001"
echo "  - Node 2: localhost:7002"
echo ""
echo "Raft ports:"
echo "  - Shard 0: 9000-9002"
echo "  - Shard 1: 9100-9102"
echo "  - Shard 2: 9200-9202"
echo ""
echo "Test with: echo 'SET key1 value1' | nc localhost 7000"
echo "Status: echo 'STATUS' | nc localhost 7000"
