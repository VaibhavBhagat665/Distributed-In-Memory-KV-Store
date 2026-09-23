#!/bin/bash

# Start a 5-node distributed KV cluster

echo "=== Starting 5-Node Distributed KV Cluster ==="
echo ""

# Kill any existing nodes
pkill -9 distributed_server 2>/dev/null

# Wait a moment
sleep 1

# Start 5 nodes in background
./bin/distributed_server -id=node1 -port=9000 -resp=6379 -base=9000 -peers=node2,node3,node4,node5 &
./bin/distributed_server -id=node2 -port=9001 -resp=6380 -base=9000 -peers=node1,node3,node4,node5 &
./bin/distributed_server -id=node3 -port=9002 -resp=6381 -base=9000 -peers=node1,node2,node4,node5 &
./bin/distributed_server -id=node4 -port=9003 -resp=6382 -base=9000 -peers=node1,node2,node3,node5 &
./bin/distributed_server -id=node5 -port=9004 -resp=6383 -base=9000 -peers=node1,node2,node3,node4 &

echo ""
echo "✓ Cluster started!"
echo ""
echo "Nodes:"
echo "  node1: Raft=9000, RESP=6379"
echo "  node2: Raft=9001, RESP=6380"
echo "  node3: Raft=9002, RESP=6381"
echo "  node4: Raft=9003, RESP=6382"
echo "  node5: Raft=9004, RESP=6383"
echo ""
echo "Wait 2 seconds for leader election..."
sleep 2
echo ""
echo "Try: redis-cli -p 6379"
echo "     > RAFT_STATUS"
echo "     > SET key1 value1"
echo "     > GET key1"
echo ""
echo "Press Ctrl+C to stop all nodes"

# Wait for Ctrl+C
trap "pkill -9 distributed_server; echo 'Cluster stopped.'; exit" INT
wait
