package sharding

import (
	"fmt"
	"math"
	"testing"
)

func TestHashRingDeterminism(t *testing.T) {
	shards := []ShardID{0, 1, 2}
	ring := NewHashRing(shards, 100)
	
	testKeys := []string{"key1", "key2", "key3", "user:123", "session:abc"}
	
	// First pass - record shard assignments
	firstPass := make(map[string]ShardID)
	for _, key := range testKeys {
		firstPass[key] = ring.GetShard(key)
	}
	
	// Second pass - verify same assignments
	for _, key := range testKeys {
		shard := ring.GetShard(key)
		if shard != firstPass[key] {
			t.Errorf("Key %s mapped to different shards: first=%d, second=%d", 
				key, firstPass[key], shard)
		}
	}
	
	t.Logf("✓ Determinism test passed: all keys consistently mapped")
}

func TestHashRingUniformDistribution(t *testing.T) {
	shards := []ShardID{0, 1, 2}
	ring := NewHashRing(shards, 100)
	
	// Generate 10,000 keys
	numKeys := 10000
	keys := make([]string, numKeys)
	for i := 0; i < numKeys; i++ {
		keys[i] = fmt.Sprintf("key%d", i)
	}
	
	// Get distribution
	distribution := ring.GetKeyDistribution(keys)
	
	// Calculate expected count per shard
	expectedPerShard := numKeys / len(shards)
	
	// Check each shard's count
	t.Logf("Key distribution across %d shards (%d keys):", len(shards), numKeys)
	for shardID := 0; shardID < len(shards); shardID++ {
		count := distribution[ShardID(shardID)]
		variance := float64(count-expectedPerShard) / float64(expectedPerShard) * 100
		t.Logf("  Shard %d: %d keys (%.2f%% variance)", shardID, count, variance)
		
		// Variance should be within 10%
		if math.Abs(variance) > 10.0 {
			t.Errorf("Shard %d has too much variance: %.2f%% (expected ≤10%%)", 
				shardID, math.Abs(variance))
		}
	}
	
	t.Logf("✓ Uniform distribution test passed")
}

func TestHashRingAddShard(t *testing.T) {
	// Start with 3 shards
	shards := []ShardID{0, 1, 2}
	ring := NewHashRing(shards, 100)
	
	// Generate keys and record initial distribution
	numKeys := 10000
	keys := make([]string, numKeys)
	for i := 0; i < numKeys; i++ {
		keys[i] = fmt.Sprintf("key%d", i)
	}
	
	initialAssignments := make(map[string]ShardID)
	for _, key := range keys {
		initialAssignments[key] = ring.GetShard(key)
	}
	
	// Add shard 3
	ring.AddShard(ShardID(3))
	
	// Count how many keys moved
	moved := 0
	for _, key := range keys {
		newShard := ring.GetShard(key)
		if newShard != initialAssignments[key] {
			moved++
		}
	}
	
	// With consistent hashing, adding 1 shard to 3 shards should move ~25% of keys
	// (1/(3+1) = 0.25)
	expectedMoveRatio := 1.0 / float64(len(shards)+1)
	actualMoveRatio := float64(moved) / float64(numKeys)
	
	t.Logf("Added shard 3: %d/%d keys moved (%.2f%%)", moved, numKeys, actualMoveRatio*100)
	t.Logf("Expected move ratio: %.2f%%, Actual: %.2f%%", 
		expectedMoveRatio*100, actualMoveRatio*100)
	
	// Allow 10% tolerance
	if math.Abs(actualMoveRatio-expectedMoveRatio) > 0.10 {
		t.Errorf("Key movement ratio %.2f is too far from expected %.2f", 
			actualMoveRatio, expectedMoveRatio)
	}
	
	t.Logf("✓ Add shard test passed: minimal key movement")
}

func TestHashRingRemoveShard(t *testing.T) {
	// Start with 3 shards
	shards := []ShardID{0, 1, 2}
	ring := NewHashRing(shards, 100)
	
	// Generate keys
	numKeys := 1000
	keys := make([]string, numKeys)
	for i := 0; i < numKeys; i++ {
		keys[i] = fmt.Sprintf("key%d", i)
	}
	
	// Get initial shard count
	initialShardCount := ring.GetShardCount()
	
	// Remove shard 1
	ring.RemoveShard(ShardID(1))
	
	// Verify shard count decreased
	if ring.GetShardCount() != initialShardCount-1 {
		t.Errorf("Expected shard count %d, got %d", 
			initialShardCount-1, ring.GetShardCount())
	}
	
	// Verify no keys map to removed shard
	for _, key := range keys {
		shard := ring.GetShard(key)
		if shard == ShardID(1) {
			t.Errorf("Key %s still maps to removed shard 1", key)
		}
	}
	
	t.Logf("✓ Remove shard test passed: shard 1 removed, keys redistributed")
}

func TestHashRingEmptyRing(t *testing.T) {
	ring := NewHashRing([]ShardID{}, 100)
	
	// Should return shard 0 for any key when ring is empty
	shard := ring.GetShard("test_key")
	if shard != 0 {
		t.Errorf("Expected shard 0 for empty ring, got %d", shard)
	}
	
	t.Logf("✓ Empty ring test passed")
}

func TestHashRingSingleShard(t *testing.T) {
	ring := NewHashRing([]ShardID{5}, 100)
	
	// All keys should map to the single shard
	keys := []string{"key1", "key2", "key3"}
	for _, key := range keys {
		shard := ring.GetShard(key)
		if shard != 5 {
			t.Errorf("Key %s mapped to shard %d, expected 5", key, shard)
		}
	}
	
	t.Logf("✓ Single shard test passed")
}

func BenchmarkHashRingGetShard(b *testing.B) {
	shards := []ShardID{0, 1, 2, 3, 4, 5, 6, 7}
	ring := NewHashRing(shards, 100)
	
	keys := make([]string, 1000)
	for i := 0; i < 1000; i++ {
		keys[i] = fmt.Sprintf("key%d", i)
	}
	
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		key := keys[i%len(keys)]
		ring.GetShard(key)
	}
}
