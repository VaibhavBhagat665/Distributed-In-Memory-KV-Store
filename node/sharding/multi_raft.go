package sharding

import (
	"fmt"
	"sync"
	"time"

	"github.com/VaibhavBhagat665/Distributed-In-Memory-KV-Store/raft"
)

// MultiRaftManager manages multiple independent Raft groups (shards)
type MultiRaftManager struct {
	shards map[ShardID]*ShardRaft
	config MultiRaftConfig
	mu     sync.RWMutex
}

// ShardRaft represents a single shard's Raft node and metadata
type ShardRaft struct {
	ShardID  ShardID
	Node     *raft.Node
	ApplyCh  chan raft.ApplyMsg
	Client   *raft.RPCClient
	Server   *raft.RPCServer
	Leader   string
	Term     uint64
	Shutdown chan struct{}
}

// MultiRaftConfig configuration for multi-raft setup
type MultiRaftConfig struct {
	NumShards         int
	NodesPerShard     int
	BaseRaftPort      int           // Shard 0 starts here, Shard 1 at BaseRaftPort+100, etc
	BaseClientPort    int           // Client-facing ports
	HeartbeatInterval time.Duration
	ElectionTimeout   time.Duration
	NodeID            int // This node's ID within each shard (0, 1, or 2)
}

// NewMultiRaftManager creates a new multi-raft manager
func NewMultiRaftManager(config MultiRaftConfig) *MultiRaftManager {
	return &MultiRaftManager{
		shards: make(map[ShardID]*ShardRaft),
		config: config,
	}
}

// StartShard initializes and starts a Raft group for a specific shard
func (mrm *MultiRaftManager) StartShard(shardID ShardID, peers []string) error {
	mrm.mu.Lock()
	defer mrm.mu.Unlock()

	// Check if shard already exists
	if _, exists := mrm.shards[shardID]; exists {
		return fmt.Errorf("shard %d already started", shardID)
	}

	// Calculate ports for this shard
	// Each shard gets isolated RPC ports
	raftPort := mrm.config.BaseRaftPort + (int(shardID) * 100) + mrm.config.NodeID

	// Create apply channel for this shard
	applyCh := make(chan raft.ApplyMsg, 100)

	// Create RPC client
	rpcClient := raft.NewRPCClient()

	// Create Raft node config
	nodeID := fmt.Sprintf("shard%d_node%d", shardID, mrm.config.NodeID)
	raftConfig := raft.Config{
		ID:                nodeID,
		Peers:             peers,
		HeartbeatInterval: mrm.config.HeartbeatInterval,
		ElectionTimeout:   mrm.config.ElectionTimeout,
	}

	// Create Raft node
	node := raft.NewNode(raftConfig, applyCh, rpcClient)

	// Create RPC server for this shard
	rpcServer := raft.NewRPCServer(node, fmt.Sprintf("localhost:%d", raftPort))
	go rpcServer.Start()

	// Store shard info
	shardRaft := &ShardRaft{
		ShardID:  shardID,
		Node:     node,
		ApplyCh:  applyCh,
		Client:   rpcClient,
		Server:   rpcServer,
		Leader:   "",
		Term:     0,
		Shutdown: make(chan struct{}),
	}

	mrm.shards[shardID] = shardRaft

	// Start leader tracking
	go mrm.trackLeader(shardID)

	return nil
}

// StopShard stops a shard's Raft group
func (mrm *MultiRaftManager) StopShard(shardID ShardID) error {
	mrm.mu.Lock()
	defer mrm.mu.Unlock()

	shard, exists := mrm.shards[shardID]
	if !exists {
		return fmt.Errorf("shard %d not found", shardID)
	}

	// Signal shutdown
	close(shard.Shutdown)

	// Stop Raft node
	shard.Node.Shutdown()

	// Stop RPC server
	shard.Server.Stop()

	// Remove from map
	delete(mrm.shards, shardID)

	return nil
}

// GetShardLeader returns the current leader address for a shard
func (mrm *MultiRaftManager) GetShardLeader(shardID ShardID) (string, error) {
	mrm.mu.RLock()
	defer mrm.mu.RUnlock()

	shard, exists := mrm.shards[shardID]
	if !exists {
		return "", fmt.Errorf("shard %d not found", shardID)
	}

	if shard.Leader == "" {
		return "", fmt.Errorf("shard %d has no leader", shardID)
	}

	return shard.Leader, nil
}

// ProposeToShard proposes a command to a specific shard's Raft group
func (mrm *MultiRaftManager) ProposeToShard(shardID ShardID, command []byte) error {
	mrm.mu.RLock()
	shard, exists := mrm.shards[shardID]
	mrm.mu.RUnlock()

	if !exists {
		return fmt.Errorf("shard %d not found", shardID)
	}

	// Propose to Raft
	return shard.Node.Propose(command)
}

// GetShardStatus returns status for a specific shard
func (mrm *MultiRaftManager) GetShardStatus(shardID ShardID) (*ShardStatus, error) {
	mrm.mu.RLock()
	defer mrm.mu.RUnlock()

	shard, exists := mrm.shards[shardID]
	if !exists {
		return nil, fmt.Errorf("shard %d not found", shardID)
	}

	state, term := shard.Node.GetState()

	return &ShardStatus{
		ShardID:     shardID,
		Leader:      shard.Leader,
		Term:        term,
		State:       string(state),
		CommitIndex: shard.Node.GetCommitIndex(),
	}, nil
}

// GetAllShardStatuses returns status for all shards
func (mrm *MultiRaftManager) GetAllShardStatuses() map[ShardID]*ShardStatus {
	mrm.mu.RLock()
	defer mrm.mu.RUnlock()

	statuses := make(map[ShardID]*ShardStatus)
	for shardID := range mrm.shards {
		if status, err := mrm.GetShardStatus(shardID); err == nil {
			statuses[shardID] = status
		}
	}

	return statuses
}

// GetApplyChannel returns the apply channel for a specific shard
func (mrm *MultiRaftManager) GetApplyChannel(shardID ShardID) (chan raft.ApplyMsg, error) {
	mrm.mu.RLock()
	defer mrm.mu.RUnlock()

	shard, exists := mrm.shards[shardID]
	if !exists {
		return nil, fmt.Errorf("shard %d not found", shardID)
	}

	return shard.ApplyCh, nil
}

// trackLeader monitors leader changes for a shard
func (mrm *MultiRaftManager) trackLeader(shardID ShardID) {
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			mrm.mu.RLock()
			shard, exists := mrm.shards[shardID]
			mrm.mu.RUnlock()

			if !exists {
				return
			}

			select {
			case <-shard.Shutdown:
				return
			default:
			}

			// Get current state
			state, term := shard.Node.GetState()

			mrm.mu.Lock()
			shard.Term = term
			if state == raft.Leader {
				// This node is the leader
				shard.Leader = shard.Node.GetID()
			}
			mrm.mu.Unlock()
		}
	}
}

// ShardStatus contains status information for a shard
type ShardStatus struct {
	ShardID     ShardID
	Leader      string
	Term        uint64
	State       string
	CommitIndex uint64
}
