# Raft Consensus Implementation

This directory contains a from-scratch implementation of the Raft consensus algorithm for distributed log replication.

## Overview

**Raft** is a consensus algorithm designed to be understandable. It provides the same guarantees as Paxos but with a structure that makes it easier to reason about.

This implementation follows the [Raft paper](https://raft.github.io/raft.pdf) (Diego Ongaro & John Ousterhout, 2014) and implements:

- ✅ Leader election with randomized timeouts
- ✅ Log replication via AppendEntries RPC
- ✅ Safety guarantees (election safety, leader append-only, log matching, state machine safety)
- ✅ HTTP-based RPC layer
- ✅ Persistent state (term, votedFor) for crash recovery
- ⚠️ Log compaction (planned - using snapshots from Phase 3)
- ⚠️ Membership changes (stretch goal)

## Architecture

### Components

**`node.go`** - Core Raft node implementation
- State machine with 3 states: Follower, Candidate, Leader
- Election timer (triggers elections)
- Heartbeat timer (leader sends periodic heartbeats)
- Log replication logic
- Commit protocol

**`rpc.go`** - Network RPC layer
- RPCServer: HTTP server handling incoming RequestVote and AppendEntries
- RPCClient: HTTP client for sending RPCs to peers
- JSON serialization (simple, can upgrade to gRPC later)

**`storage.go`** - Persistent storage
- `MemoryLogStore`: In-memory log (MVP, fast for testing)
- `FileStateStore`: Disk-persisted term/votedFor (survives crashes)

**`cluster.go`** - Cluster management utilities
- Creates N-node clusters for testing
- Manages apply loops (committed entries → state machine)

**`types.go`** - Data structures and interfaces
- LogEntry, AppendEntriesRequest/Response, RequestVoteRequest/Response
- LogStore and StateStore interfaces

## How Raft Works

### 1. Leader Election

- All nodes start as **Followers**
- If a follower doesn't hear from a leader within the election timeout, it becomes a **Candidate**
- Candidate increments term, votes for itself, and requests votes from peers
- If it receives votes from a majority, it becomes the **Leader**
- Leader sends periodic heartbeats to prevent new elections

**Key insight:** Randomized election timeouts prevent split votes.

### 2. Log Replication

- Clients send commands to the leader
- Leader appends the command to its log
- Leader sends AppendEntries RPCs to all followers
- Once a majority of followers acknowledge, the entry is **committed**
- Leader applies committed entries to its state machine
- Followers apply entries when leader tells them it's committed

**Key insight:** A log entry is only committed after being replicated to a majority.

### 3. Safety Properties

The implementation enforces these critical safety properties:

1. **Election Safety:** At most one leader per term
2. **Leader Append-Only:** Leader never deletes or overwrites entries
3. **Log Matching:** If two logs contain an entry with same index and term, all preceding entries are identical
4. **Leader Completeness:** If an entry is committed in term T, it will be present in all leaders for terms > T
5. **State Machine Safety:** If a node has applied an entry at index i, no other node will apply a different entry at i

## Usage

### Starting a Cluster

```go
import "github.com/yourusername/kvstore/raft"

// Create 5-node cluster on ports 9000-9004
nodeIDs := []string{"node1", "node2", "node3", "node4", "node5"}
cluster, err := raft.NewCluster(nodeIDs, 9000)
if err != nil {
    log.Fatal(err)
}
defer cluster.Shutdown()

// Wait for leader election
time.Sleep(2 * time.Second)

// Find leader
leader, leaderID := cluster.GetLeader()
if leader != nil {
    fmt.Printf("Leader: %s\n", leaderID)
}
```

### Proposing Commands

```go
// Propose a command (must be leader)
command := []byte("SET key1 value1")
index, term, isLeader := leader.Propose(command)

if !isLeader {
    fmt.Println("Not the leader!")
} else {
    fmt.Printf("Accepted at index %d, term %d\n", index, term)
}
```

### Applying Commands

```go
// Create apply channel
applyCh := make(chan raft.ApplyMsg, 100)

// Create node
node := raft.NewNode(config, applyCh, rpcClient)

// Process committed entries
go func() {
    for msg := range applyCh {
        fmt.Printf("Apply: index=%d, command=%s\n", msg.Index, string(msg.Command))
        // Apply to your state machine (e.g., KV store)
    }
}()
```

## Testing

### Unit Tests

```bash
go test ./raft -v
```

Tests:
- `TestLeaderElection` - Verifies exactly one leader is elected
- `TestLogReplication` - Verifies commands are replicated to followers
- `TestBasicConsensus` - End-to-end consensus test

### Interactive Demo

```bash
go build -o bin/raft_test ./cmd/raft_test
./bin/raft_test
```

This starts a 5-node cluster, elects a leader, and proposes several commands. You can observe the logs to see:
- Election messages
- Heartbeats
- Command proposals
- Apply messages on each node

### Failover Test (Critical for MVP)

```bash
# Start cluster
./bin/raft_test

# In another terminal, kill the leader
ps aux | grep raft_test
kill -9 <LEADER_PID>

# Observe: new leader elected within ~1 second
```

**Expected:**
- Followers detect leader failure (election timeout)
- One follower becomes candidate and requests votes
- New leader elected (term incremented)
- Cluster continues accepting commands
- No committed data lost

## Configuration

```go
config := raft.Config{
    ID:                "node1",           // Unique node ID
    Peers:             []string{"node2", "node3", "node4", "node5"},
    ElectionTimeout:   300 * time.Millisecond,  // Base timeout (randomized)
    HeartbeatInterval: 100 * time.Millisecond,  // Leader heartbeat frequency
    LogStore:          logStore,          // Persistent log storage
    StateStore:        stateStore,        // Persistent state storage
}
```

**Tuning guidelines:**
- `ElectionTimeout` should be >> round-trip time between nodes
- Typical: 150-300ms election timeout, 50-100ms heartbeat
- `HeartbeatInterval` should be much less than `ElectionTimeout`
- Rule of thumb: `ElectionTimeout >= 10 * HeartbeatInterval`

## Performance Considerations

### Throughput

- **Batching:** Leader can batch multiple commands in one AppendEntries RPC
- **Pipelining:** Leader sends AppendEntries without waiting for responses (tracks in-flight)
- **Parallel replication:** Leader sends to all followers concurrently

Current implementation: Simple, no batching/pipelining (MVP).

### Latency

Commit latency = 1.5 RTTs:
1. Client → Leader (command proposal)
2. Leader → Followers (AppendEntries)
3. Followers → Leader (acknowledgment)

Optimization opportunities:
- **Read leases:** Leader can serve reads without quorum (linearizable)
- **Follower reads:** Allow stale reads on followers (faster, not linearizable)

### Disk I/O

Raft requires durable writes before responding:
- Leader must persist log entry before replicating
- Followers must persist log entry before acknowledging

Optimization opportunities:
- **Batch disk writes:** fsync every 10ms instead of per-write
- **Parallel I/O:** Replicate and disk-write concurrently

Current implementation: No disk-based log yet (MemoryLogStore). StateStore is disk-persisted.

## Integration with KV Store

To integrate Raft with the C++ KV engine:

```go
// 1. Create apply channel
applyCh := make(chan raft.ApplyMsg, 100)

// 2. Start Raft node
node := raft.NewNode(config, applyCh, rpcClient)

// 3. Create KV engine (from Phase 5)
engine := engine.NewEngine(4, 1024*1024*1024, 0)
defer engine.Destroy()

// 4. Apply loop: committed Raft entries → KV engine
go func() {
    for msg := range applyCh {
        // Parse command (e.g., "SET key value")
        parts := strings.Fields(string(msg.Command))
        cmd := parts[0]
        
        switch cmd {
        case "SET":
            engine.Set(parts[1], parts[2], 0)
        case "GET":
            val, _ := engine.Get(parts[1])
            // Store result somewhere
        case "DEL":
            engine.Delete(parts[1])
        }
    }
}()

// 5. Client writes go through Raft
func handleSet(key, value string) error {
    cmd := fmt.Sprintf("SET %s %s", key, value)
    _, _, isLeader := node.Propose([]byte(cmd))
    if !isLeader {
        return errors.New("not leader")
    }
    return nil
}
```

## Next Steps

For **Phase 6 completion (MVP)**:
1. ✅ Core Raft algorithm
2. ✅ RPC layer
3. ✅ Persistent state
4. ⏳ Integrate with KV store (apply loop → C++ engine)
5. ⏳ End-to-end test: 5-node cluster, write-read-failover
6. ⏳ Measure: leader failover time (target: <1s)

For **Phase 7 (stretch - Multi-Raft sharding)**:
- Multiple Raft groups per node
- Consistent hashing for key → raft group mapping
- Cross-shard operations (if needed)

## References

- [Raft paper](https://raft.github.io/raft.pdf) - Original paper (very readable!)
- [Raft visualization](https://raft.github.io/) - Interactive visualization
- [etcd Raft](https://github.com/etcd-io/raft) - Production Raft in Go (but we build from scratch!)
- [Raft FAQ](https://github.com/ongardie/dissertation/blob/master/faq.md) - Common questions

## License

Part of the distributed KV store portfolio project.
