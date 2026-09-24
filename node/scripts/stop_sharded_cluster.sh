#!/bin/bash

echo "=== Stopping Sharded KV Cluster ==="

# Kill all sharded_server processes
pkill -f sharded_server

echo "✓ All sharded server processes terminated"
