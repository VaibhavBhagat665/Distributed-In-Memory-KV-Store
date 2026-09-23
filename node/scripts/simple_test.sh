#!/bin/bash

# Simple test using echo and bash TCP sockets

echo "=== Testing Distributed KV Store ==="
echo ""

# Function to send command
test_cmd() {
    port=$1
    cmd=$2
    echo "$cmd" | timeout 1 bash -c "exec 3<>/dev/tcp/localhost/$port; cat >&3; cat <&3" 2>/dev/null
}

echo "1. Checking Raft status..."
test_cmd 6379 "RAFT_STATUS"
echo ""

echo "2. Writing data (SET key1 value1)..."
test_cmd 6379 "SET key1 value1"
echo ""

sleep 1

echo "3. Reading from node1 (port 6379)..."
test_cmd 6379 "GET key1"
echo ""

echo "4. Reading from node2 (port 6380)..."
test_cmd 6380 "GET key1"
echo ""

echo "5. Writing more keys..."
test_cmd 6379 "SET key2 hello"
test_cmd 6379 "SET key3 world"
echo ""

sleep 1

echo "6. Reading from node3 (port 6381)..."
test_cmd 6381 "GET key2"
test_cmd 6381 "GET key3"
echo ""

echo "✓ Test complete!"
