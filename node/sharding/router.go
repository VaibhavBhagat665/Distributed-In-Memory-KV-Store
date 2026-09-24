package sharding

import (
	"fmt"
	"sync"
	"time"
)

// ShardRouter routes client requests to the correct shard based on key
type ShardRouter struct {
	hashRing     *HashRing
	multiRaft    *MultiRaftManager
	shardLeaders map[ShardID]string // shardID -> leader address
	mu           sync.RWMutex
	maxRetries   int
	retryDelay   time.Duration
}

// RouterConfig configuration for shard router
type RouterConfig struct {
	HashRing   *HashRing
	MultiRaft  *MultiRaftManager
	MaxRetries int
	RetryDelay time.Duration
}

// Operation types for routing
type Operation struct {
	Type  string   // SET, GET, DEL
	Key   string
	Value string
	TTL   uint64
}

// Result from operation
type Result struct {
	Success bool
	Value   string
	Error   error
}

// NewShardRouter creates a new shard router
func NewShardRouter(config RouterConfig) *ShardRouter {
	maxRetries := config.MaxRetries
	if maxRetries == 0 {
		maxRetries = 3 // Default
	}

	retryDelay := config.RetryDelay
	if retryDelay == 0 {
		retryDelay = 50 * time.Millisecond // Default
	}

	sr := &ShardRouter{
		hashRing:     config.HashRing,
		multiRaft:    config.MultiRaft,
		shardLeaders: make(map[ShardID]string),
		maxRetries:   maxRetries,
		retryDelay:   retryDelay,
	}

	// Start leader tracking
	go sr.trackLeaders()

	return sr
}

// Route routes an operation to the correct shard
func (sr *ShardRouter) Route(op Operation) (Result, error) {
	// Determine target shard based on key
	shardID := sr.hashRing.GetShard(op.Key)

	// Try to execute operation with retries
	for attempt := 0; attempt < sr.maxRetries; attempt++ {
		result, err := sr.executeOnShard(shardID, op)
		
		if err == nil {
			return result, nil
		}

		// If "not leader" error, update leader and retry
		if isNotLeaderError(err) {
			sr.updateLeader(shardID)
			time.Sleep(sr.retryDelay)
			continue
		}

		// If shard unavailable, return error immediately
		if isShardUnavailableError(err) {
			return Result{Success: false, Error: err}, err
		}

		// Other errors - retry with delay
		time.Sleep(sr.retryDelay)
	}

	return Result{Success: false, Error: fmt.Errorf("operation failed after %d retries", sr.maxRetries)}, 
		fmt.Errorf("max retries exceeded for shard %d", shardID)
}

// executeOnShard executes an operation on a specific shard
func (sr *ShardRouter) executeOnShard(shardID ShardID, op Operation) (Result, error) {
	// Serialize operation as command
	command := sr.serializeOperation(op)

	// Propose to shard's Raft
	err := sr.multiRaft.ProposeToShard(shardID, command)
	if err != nil {
		return Result{Success: false, Error: err}, err
	}

	// For MVP, we assume success after proposal
	// Production version would wait for commit confirmation via apply channel
	return Result{Success: true, Value: ""}, nil
}

// serializeOperation converts an operation to bytes
func (sr *ShardRouter) serializeOperation(op Operation) []byte {
	// Simple serialization: "TYPE key value ttl"
	switch op.Type {
	case "SET":
		return []byte(fmt.Sprintf("SET %s %s %d", op.Key, op.Value, op.TTL))
	case "GET":
		return []byte(fmt.Sprintf("GET %s", op.Key))
	case "DEL":
		return []byte(fmt.Sprintf("DEL %s", op.Key))
	default:
		return []byte(fmt.Sprintf("%s %s", op.Type, op.Key))
	}
}

// UpdateLeader manually updates the leader for a shard
func (sr *ShardRouter) UpdateLeader(shardID ShardID, leaderAddr string) {
	sr.mu.Lock()
	defer sr.mu.Unlock()
	sr.shardLeaders[shardID] = leaderAddr
}

// updateLeader refreshes leader info from MultiRaft
func (sr *ShardRouter) updateLeader(shardID ShardID) {
	leader, err := sr.multiRaft.GetShardLeader(shardID)
	if err == nil && leader != "" {
		sr.UpdateLeader(shardID, leader)
	}
}

// GetShardForKey returns which shard a key belongs to
func (sr *ShardRouter) GetShardForKey(key string) ShardID {
	return sr.hashRing.GetShard(key)
}

// GetShardStatus returns status for all shards
func (sr *ShardRouter) GetShardStatus() map[ShardID]*ShardStatus {
	return sr.multiRaft.GetAllShardStatuses()
}

// GetLeader returns the current leader for a shard
func (sr *ShardRouter) GetLeader(shardID ShardID) (string, error) {
	sr.mu.RLock()
	leader, exists := sr.shardLeaders[shardID]
	sr.mu.RUnlock()

	if !exists || leader == "" {
		return "", fmt.Errorf("no leader found for shard %d", shardID)
	}

	return leader, nil
}

// trackLeaders periodically updates leader information from MultiRaft
func (sr *ShardRouter) trackLeaders() {
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for range ticker.C {
		// Get all shard statuses
		statuses := sr.multiRaft.GetAllShardStatuses()

		sr.mu.Lock()
		for shardID, status := range statuses {
			if status.Leader != "" {
				sr.shardLeaders[shardID] = status.Leader
			}
		}
		sr.mu.Unlock()
	}
}

// Helper functions for error classification
func isNotLeaderError(err error) bool {
	// Check if error message contains "not leader"
	return err != nil && (err.Error() == "not leader" || 
		fmt.Sprint(err) == "not leader")
}

func isShardUnavailableError(err error) bool {
	// Check if error indicates shard is unavailable
	return err != nil && (err.Error() == "shard unavailable" || 
		fmt.Sprint(err) == "no leader")
}

// GetKeyDistribution returns how many keys would map to each shard
// Useful for monitoring load balance
func (sr *ShardRouter) GetKeyDistribution(keys []string) map[ShardID]int {
	return sr.hashRing.GetKeyDistribution(keys)
}
