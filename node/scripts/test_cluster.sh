#!/bin/bash

echo "=== Testing Distributed KV Cluster ==="
echo ""

# Function to send command to a node
send_cmd() {
    port=$1
    cmd=$2
    echo "$cmd" | nc localhost $port | head -n 2
}

# Test 1: Check Raft status
echo "1. Checking Raft status on all nodes..."
for port in 6379 6380 6381 6382 6383; do
    echo -n "  Port $port: "
    send_cmd $port "RAFT_STATUS"
done
echo ""

# Test 2: Find leader and write data
echo "2. Writing data through leader..."
echo "  SET key1 value1" 
send_cmd 6379 "SET key1 value1"
echo ""

sleep 1

# Test 3: Read from all nodes
echo "3. Reading key1 from all nodes (should be replicated)..."
for port in 6379 6380 6381 6382 6383; do
    echo -n "  Port $port: "
    send_cmd $port "GET key1"
done
echo ""

# Test 4: Write more data
echo "4. Writing more keys..."
send_cmd 6379 "SET key2 value2"
send_cmd 6379 "SET key3 value3"
echo ""

sleep 1

# Test 5: Read from a follower
echo "5. Reading from follower (node2 port 6380)..."
echo -n "  GET key2: "
send_cmd 6380 "GET key2"
echo -n "  GET key3: "
send_cmd 6380 "GET key3"
echo ""

echo "✓ Test complete!"
