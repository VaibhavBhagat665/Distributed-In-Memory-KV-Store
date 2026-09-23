package raft

import (
	"fmt"
	"time"
)

// Cluster manages a Raft cluster
type Cluster struct {
	nodes   map[string]*Node
	servers map[string]*RPCServer
}

// NewCluster creates a new Raft cluster
func NewCluster(nodeIDs []string, basePort int) (*Cluster, error) {
	// Build peer address map
	peerAddrs := make(map[string]string)
	for i, id := range nodeIDs {
		peerAddrs[id] = fmt.Sprintf("localhost:%d", basePort+i)
	}
	
	cluster := &Cluster{
		nodes:   make(map[string]*Node),
		servers: make(map[string]*RPCServer),
	}
	
	// Create each node
	for _, id := range nodeIDs {
		// Build peer list (exclude self)
		peers := make([]string, 0, len(nodeIDs)-1)
		for _, peerID := range nodeIDs {
			if peerID != id {
				peers = append(peers, peerID)
			}
		}
		
		// Create node configuration
		config := Config{
			ID:                id,
			Peers:             peers,
			ElectionTimeout:   300 * time.Millisecond,
			HeartbeatInterval: 100 * time.Millisecond,
		}
		
		// Create apply channel
		applyCh := make(chan ApplyMsg, 100)
		
		// Create RPC client
		rpcClient := NewRPCClient(peerAddrs)
		
		// Create node
		node := NewNode(config, applyCh, rpcClient)
		cluster.nodes[id] = node
		
		// Create RPC server
		addr := peerAddrs[id]
		server := NewRPCServer(node, addr)
		cluster.servers[id] = server
		
		// Start RPC server in background
		go func(srv *RPCServer, nodeID string) {
			if err := srv.Start(); err != nil {
				fmt.Printf("[%s] RPC server error: %v\n", nodeID, err)
			}
		}(server, id)
		
		// Start apply loop
		go func(nodeID string, ch chan ApplyMsg) {
			for msg := range ch {
				fmt.Printf("[%s] Applied command at index %d: %s\n", 
					nodeID, msg.Index, string(msg.Command))
			}
		}(id, applyCh)
	}
	
	return cluster, nil
}

// GetNode returns a node by ID
func (c *Cluster) GetNode(id string) *Node {
	return c.nodes[id]
}

// GetLeader finds the current leader
func (c *Cluster) GetLeader() (*Node, string) {
	for id, node := range c.nodes {
		_, isLeader := node.GetState()
		if isLeader {
			return node, id
		}
	}
	return nil, ""
}

// Shutdown shuts down all nodes
func (c *Cluster) Shutdown() {
	for _, node := range c.nodes {
		node.Shutdown()
	}
}
