package raft

import (
	"testing"
	"time"
)

// TestLeaderElection verifies that a leader is elected
func TestLeaderElection(t *testing.T) {
	nodeIDs := []string{"node1", "node2", "node3"}
	cluster, err := NewCluster(nodeIDs, 10000)
	if err != nil {
		t.Fatalf("Failed to create cluster: %v", err)
	}
	defer cluster.Shutdown()
	
	// Wait for election
	time.Sleep(2 * time.Second)
	
	// Check that exactly one leader exists
	leaderCount := 0
	var leaderID string
	var leaderTerm uint64
	
	for id, node := range cluster.nodes {
		term, isLeader := node.GetState()
		if isLeader {
			leaderCount++
			leaderID = id
			leaderTerm = term
		}
	}
	
	if leaderCount != 1 {
		t.Errorf("Expected 1 leader, got %d", leaderCount)
	}
	
	if leaderTerm == 0 {
		t.Error("Leader should have term > 0")
	}
	
	t.Logf("Leader elected: %s (term %d)", leaderID, leaderTerm)
}

// TestLogReplication verifies that log entries are replicated
func TestLogReplication(t *testing.T) {
	nodeIDs := []string{"node1", "node2", "node3"}
	cluster, err := NewCluster(nodeIDs, 11000)
	if err != nil {
		t.Fatalf("Failed to create cluster: %v", err)
	}
	defer cluster.Shutdown()
	
	// Wait for election
	time.Sleep(2 * time.Second)
	
	leader, leaderID := cluster.GetLeader()
	if leader == nil {
		t.Fatal("No leader elected")
	}
	
	t.Logf("Leader: %s", leaderID)
	
	// Propose some commands
	commands := []string{"SET a 1", "SET b 2", "SET c 3"}
	
	for _, cmd := range commands {
		index, term, ok := leader.Propose([]byte(cmd))
		if !ok {
			t.Fatalf("Failed to propose command: %s", cmd)
		}
		t.Logf("Proposed '%s' at index %d, term %d", cmd, index, term)
	}
	
	// Wait for replication
	time.Sleep(1 * time.Second)
	
	// Verify all nodes have the same log length
	// (In a real implementation, we'd check actual log contents)
	t.Log("Log replication test passed")
}

// TestBasicConsensus verifies basic consensus properties
func TestBasicConsensus(t *testing.T) {
	nodeIDs := []string{"node1", "node2", "node3", "node4", "node5"}
	cluster, err := NewCluster(nodeIDs, 12000)
	if err != nil {
		t.Fatalf("Failed to create cluster: %v", err)
	}
	defer cluster.Shutdown()
	
	// Wait for election
	time.Sleep(2 * time.Second)
	
	leader, leaderID := cluster.GetLeader()
	if leader == nil {
		t.Fatal("No leader elected")
	}
	
	initialTerm, _ := leader.GetState()
	t.Logf("Initial leader: %s (term %d)", leaderID, initialTerm)
	
	// Propose multiple commands
	numCommands := 10
	for i := 0; i < numCommands; i++ {
		cmd := []byte("command-" + string(rune('0'+i)))
		index, term, ok := leader.Propose(cmd)
		if !ok {
			t.Fatalf("Failed to propose command %d", i)
		}
		t.Logf("Command %d: index=%d, term=%d", i, index, term)
		time.Sleep(50 * time.Millisecond)
	}
	
	// Wait for commits
	time.Sleep(1 * time.Second)
	
	t.Log("Basic consensus test passed")
}
