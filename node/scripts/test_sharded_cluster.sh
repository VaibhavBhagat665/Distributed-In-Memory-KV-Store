#!/bin/bash

echo "=== Testing Sharded KV Cluster ==="
echo ""

PORT=7000

# Test 1: Write to different shards
echo "1. Writing keys that map to different shards..."
echo "SET key1 value1" | nc -w 1 localhost $PORT
echo "SET key2 value2" | nc -w 1 localhost $PORT
echo "SET key3 value3" | nc -w 1 localhost $PORT
echo "SET user:100 alice" | nc -w 1 localhost $PORT
echo "SET user:200 bob" | nc -w 1 localhost $PORT

sleep 1

# Test 2: Read back keys
echo ""
echo "2. Reading keys back..."
echo "GET key1" | nc -w 1 localhost $PORT
echo "GET key2" | nc -w 1 localhost $PORT
echo "GET key3" | nc -w 1 localhost $PORT
echo "GET user:100" | nc -w 1 localhost $PORT
echo "GET user:200" | nc -w 1 localhost $PORT

# Test 3: Check status
echo ""
echo "3. Checking shard status..."
echo "STATUS" | nc -w 1 localhost $PORT

# Test 4: Delete a key
echo ""
echo "4. Deleting key1..."
echo "DEL key1" | nc -w 1 localhost $PORT

# Verify deletion
echo "GET key1" | nc -w 1 localhost $PORT

echo ""
echo "✓ Test complete!"
