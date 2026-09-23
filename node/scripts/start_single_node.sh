#!/bin/bash

# Start a single-node cluster (no voting needed - instant leader)

echo "=== Starting Single-Node Distributed KV Store ==="
echo ""

# Kill any existing nodes
pkill -9 distributed_server 2>/dev/null
sleep 1

# Start 1 node (will become leader immediately since no peers)
./bin/distributed_server -id=node1 -port=9000 -resp=6379 -base=9000 -peers="" &

echo ""
echo "✓ Node started!"
echo ""
echo "Node: Raft=9000, RESP=6379"
echo ""
echo "Waiting 1 second..."
sleep 1
echo ""
echo "Try: echo 'SET key1 value1' | timeout 1 bash -c \"exec 3<>/dev/tcp/localhost/6379; cat >&3; cat <&3\""
echo ""
echo "Press Ctrl+C to stop"

trap "pkill -9 distributed_server; echo 'Node stopped.'; exit" INT
wait
