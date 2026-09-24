package main

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"
	
	"github.com/yourusername/kvstore/raft"
)

func main() {
	fmt.Println("=== Raft Cluster Test ===")
	fmt.Println()
	
	// Create 5-node cluster
	nodeIDs := []string{"node1", "node2", "node3", "node4", "node5"}
	basePort := 9000
	
	fmt.Printf("Starting %d-node cluster on ports %d-%d...\n", 
		len(nodeIDs), basePort, basePort+len(nodeIDs)-1)
	
	cluster, err := raft.NewCluster(nodeIDs, basePort)
	if err != nil {
		fmt.Printf("Failed to create cluster: %v\n", err)
		os.Exit(1)
	}
	defer cluster.Shutdown()
	
	// Wait for leader election
	fmt.Println("\nWaiting for leader election...")
	time.Sleep(2 * time.Second)
	
	leader, leaderID := cluster.GetLeader()
	if leader == nil {
		fmt.Println("No leader elected yet, waiting longer...")
		time.Sleep(3 * time.Second)
		leader, leaderID = cluster.GetLeader()
	}
	
	if leader != nil {
		term, _ := leader.GetState()
		fmt.Printf("\n✓ Leader elected: %s (term %d)\n", leaderID, term)
	} else {
		fmt.Println("\n✗ No leader elected")
	}
	
	// Submit commands if we have a leader
	if leader != nil {
		fmt.Println("\n=== Testing Log Replication ===")
		
		commands := []string{
			"SET key1 value1",
			"SET key2 value2",
			"GET key1",
			"DEL key1",
			"SET key3 value3",
		}
		
		for i, cmd := range commands {
			fmt.Printf("\n[%d] Proposing: %s\n", i+1, cmd)
			
			index, term, ok := leader.Propose([]byte(cmd))
			if !ok {
				fmt.Printf("  ✗ Failed to propose (not leader)\n")
				continue
			}
			
			fmt.Printf("  ✓ Accepted at index %d, term %d\n", index, term)
			
			// Give some time for replication
			time.Sleep(200 * time.Millisecond)
		}
		
		fmt.Println("\n=== Commands Submitted ===")
		fmt.Println("Watch the logs above for 'Applied command' messages")
		fmt.Println("Each node should apply the commands in the same order")
	}
	
	// Keep running until interrupted
	fmt.Println("\nCluster is running. Press Ctrl+C to stop.")
	fmt.Println("\nTry killing the leader process to test failover!")
	fmt.Printf("Leader PID: %d\n", os.Getpid())
	
	// Wait for interrupt
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan
	
	fmt.Println("\n\nShutting down cluster...")
}
