package sharding

import (
	"fmt"
	"hash/fnv"
	"sort"
	"sync"
)

// ShardID represents a unique identifier for a shard
type ShardID int

// VirtualNode represents a point on the consistent hash ring
type VirtualNode struct {
	Hash    uint64
	ShardID ShardID
}

// HashRing implements consistent hashing with virtual nodes
type HashRing struct {
	shards       []ShardID
	virtualNodes int
	ring         []VirtualNode
	mu           sync.RWMutex
}

// NewHashRing creates a new consistent hash ring
func NewHashRing(shards []ShardID, virtualNodes int) *HashRing {
	hr := &HashRing{
		shards:       make([]ShardID, len(shards)),
		virtualNodes: virtualNodes,
		ring:         make([]VirtualNode, 0, len(shards)*virtualNodes),
	}
	
	copy(hr.shards, shards)
	
	// Create virtual nodes for each shard
	for _, shardID := range shards {
		hr.addShardToRing(shardID)
	}
	
	// Sort ring by hash value
	sort.Slice(hr.ring, func(i, j int) bool {
		return hr.ring[i].Hash < hr.ring[j].Hash
	})
	
	return hr
}

// GetShard returns the shard ID for a given key using consistent hashing
func (hr *HashRing) GetShard(key string) ShardID {
	hr.mu.RLock()
	defer hr.mu.RUnlock()
	
	if len(hr.ring) == 0 {
		return 0
	}
	
	keyHash := hr.hash(key)
	
	// Binary search for the first virtual node with hash >= keyHash
	idx := sort.Search(len(hr.ring), func(i int) bool {
		return hr.ring[i].Hash >= keyHash
	})
	
	// Wrap around if we've gone past the end
	if idx >= len(hr.ring) {
		idx = 0
	}
	
	return hr.ring[idx].ShardID
}

// AddShard adds a new shard to the hash ring
func (hr *HashRing) AddShard(shardID ShardID) {
	hr.mu.Lock()
	defer hr.mu.Unlock()
	
	// Check if shard already exists
	for _, s := range hr.shards {
		if s == shardID {
			return
		}
	}
	
	hr.shards = append(hr.shards, shardID)
	hr.addShardToRing(shardID)
	
	// Re-sort ring
	sort.Slice(hr.ring, func(i, j int) bool {
		return hr.ring[i].Hash < hr.ring[j].Hash
	})
}

// RemoveShard removes a shard from the hash ring
func (hr *HashRing) RemoveShard(shardID ShardID) {
	hr.mu.Lock()
	defer hr.mu.Unlock()
	
	// Remove from shards list
	for i, s := range hr.shards {
		if s == shardID {
			hr.shards = append(hr.shards[:i], hr.shards[i+1:]...)
			break
		}
	}
	
	// Remove virtual nodes from ring
	newRing := make([]VirtualNode, 0, len(hr.ring))
	for _, vn := range hr.ring {
		if vn.ShardID != shardID {
			newRing = append(newRing, vn)
		}
	}
	hr.ring = newRing
}

// GetShardCount returns the number of shards in the ring
func (hr *HashRing) GetShardCount() int {
	hr.mu.RLock()
	defer hr.mu.RUnlock()
	return len(hr.shards)
}

// GetShards returns a copy of all shard IDs
func (hr *HashRing) GetShards() []ShardID {
	hr.mu.RLock()
	defer hr.mu.RUnlock()
	
	shards := make([]ShardID, len(hr.shards))
	copy(shards, hr.shards)
	return shards
}

// addShardToRing adds virtual nodes for a shard to the ring (not thread-safe)
func (hr *HashRing) addShardToRing(shardID ShardID) {
	for i := 0; i < hr.virtualNodes; i++ {
		vnodeKey := fmt.Sprintf("shard%d_vnode%d", shardID, i)
		hash := hr.hash(vnodeKey)
		hr.ring = append(hr.ring, VirtualNode{
			Hash:    hash,
			ShardID: shardID,
		})
	}
}

// hash computes a hash of a string using FNV-1a with better mixing
func (hr *HashRing) hash(key string) uint64 {
	h := fnv.New64a()
	h.Write([]byte(key))
	hash := h.Sum64()
	
	// Additional mixing to improve distribution
	// MurmurHash3-style mixing
	hash ^= hash >> 33
	hash *= 0xff51afd7ed558ccd
	hash ^= hash >> 33
	hash *= 0xc4ceb9fe1a85ec53
	hash ^= hash >> 33
	
	return hash
}

// GetKeyDistribution returns the count of keys per shard for a given set of keys
// This is useful for testing and monitoring load balance
func (hr *HashRing) GetKeyDistribution(keys []string) map[ShardID]int {
	distribution := make(map[ShardID]int)
	
	for _, key := range keys {
		shardID := hr.GetShard(key)
		distribution[shardID]++
	}
	
	return distribution
}
