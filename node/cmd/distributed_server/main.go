package main

import (
	"bufio"
	"flag"
	"fmt"
	"net"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/yourusername/kvstore/engine"
	"github.com/yourusername/kvstore/raft"
)

var (
	nodeID   = flag.String("id", "node1", "Node ID")
	httpPort = flag.Int("port", 9000, "Raft HTTP port")
	respPort = flag.Int("resp", 6379, "RESP server port")
	peers    = flag.String("peers", "node2,node3,node4,node5", "Comma-separated peer IDs")
	basePort = flag.Int("base", 9000, "Base port for Raft cluster")
	threads  = flag.Int("threads", 4, "Number of threads")
	memory   = flag.Uint64("memory", 1024*1024*1024, "Memory limit in bytes")
)

func main() {
	flag.Parse()

	fmt.Printf("=== Distributed KV Store Node ===\n")
	fmt.Printf("Node ID: %s\n", *nodeID)
	fmt.Printf("Raft port: %d\n", *httpPort)
	fmt.Printf("RESP port: %d\n", *respPort)
	fmt.Printf("Peers: %s\n", *peers)
	fmt.Println()

	// Parse peer list
	peerList := []string{}
	if *peers != "" {
		peerList = strings.Split(*peers, ",")
	}

	// Create C++ KV engine
	fmt.Printf("[%s] Initializing storage engine (%d threads, %d MB)...\n",
		*nodeID, *threads, *memory/(1024*1024))
	kvEngine, err := engine.New(*threads, *memory)
	if err != nil {
		fmt.Printf("Failed to create engine: %v\n", err)
		os.Exit(1)
	}
	defer kvEngine.Close()

	// Build peer address map for Raft
	allNodes := append([]string{*nodeID}, peerList...)
	peerAddrs := make(map[string]string)
	for i, id := range allNodes {
		peerAddrs[id] = fmt.Sprintf("localhost:%d", *basePort+i)
	}

	// Create Raft node
	applyCh := make(chan raft.ApplyMsg, 100)
	rpcClient := raft.NewRPCClient(peerAddrs)

	config := raft.Config{
		ID:                *nodeID,
		Peers:             peerList,
		ElectionTimeout:   300 * time.Millisecond,
		HeartbeatInterval: 100 * time.Millisecond,
	}

	fmt.Printf("[%s] Starting Raft node...\n", *nodeID)
	raftNode := raft.NewNode(config, applyCh, rpcClient)
	defer raftNode.Shutdown()

	// Start Raft RPC server
	rpcServer := raft.NewRPCServer(raftNode, peerAddrs[*nodeID])
	go func() {
		if err := rpcServer.Start(); err != nil {
			fmt.Printf("[%s] RPC server error: %v\n", *nodeID, err)
		}
	}()

	// Apply loop: Raft committed entries → KV engine
	go func() {
		for msg := range applyCh {
			applyCommand(kvEngine, msg, *nodeID)
		}
	}()

	// Start RESP server
	fmt.Printf("[%s] Starting RESP server on :%d...\n", *nodeID, *respPort)
	listener, err := net.Listen("tcp", fmt.Sprintf(":%d", *respPort))
	if err != nil {
		fmt.Printf("Failed to start RESP server: %v\n", err)
		os.Exit(1)
	}
	defer listener.Close()

	// Accept connections in background
	go func() {
		for {
			conn, err := listener.Accept()
			if err != nil {
				continue
			}
			go handleClient(conn, kvEngine, raftNode, *nodeID)
		}
	}()

	fmt.Printf("[%s] Node ready! Waiting for leader election...\n", *nodeID)
	fmt.Println()

	// Wait for shutdown signal
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan

	fmt.Printf("\n[%s] Shutting down...\n", *nodeID)
}

func applyCommand(kvEngine *engine.Engine, msg raft.ApplyMsg, nodeID string) {
	cmdStr := string(msg.Command)
	parts := strings.Fields(cmdStr)
	if len(parts) == 0 {
		return
	}

	cmd := strings.ToUpper(parts[0])
	
	switch cmd {
	case "SET":
		if len(parts) >= 3 {
			key := parts[1]
			value := parts[2]
			ttl := time.Duration(0)
			
			// Check for TTL options (EX seconds)
			if len(parts) >= 5 && strings.ToUpper(parts[3]) == "EX" {
				var seconds int
				fmt.Sscanf(parts[4], "%d", &seconds)
				ttl = time.Duration(seconds) * time.Second
			}
			
			err := kvEngine.Set(key, value, ttl)
			if err != nil {
				fmt.Printf("[%s] Apply SET failed: %v\n", nodeID, err)
			} else {
				fmt.Printf("[%s] Applied: SET %s (index %d)\n", nodeID, key, msg.Index)
			}
		}
		
	case "DEL":
		if len(parts) >= 2 {
			key := parts[1]
			err := kvEngine.Delete(key)
			if err != nil {
				fmt.Printf("[%s] Apply DEL failed: %v\n", nodeID, err)
			} else {
				fmt.Printf("[%s] Applied: DEL %s (index %d)\n", nodeID, key, msg.Index)
			}
		}
		
	default:
		// Ignore other commands (GET doesn't need replication)
	}
}

func handleClient(conn net.Conn, kvEngine *engine.Engine, raftNode *raft.Node, nodeID string) {
	defer conn.Close()
	
	scanner := bufio.NewScanner(conn)
	
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		
		parts := strings.Fields(line)
		if len(parts) == 0 {
			continue
		}
		
		cmd := strings.ToUpper(parts[0])
		
		switch cmd {
		case "PING":
			conn.Write([]byte("+PONG\r\n"))
			
		case "GET":
			if len(parts) < 2 {
				conn.Write([]byte("-ERR wrong number of arguments\r\n"))
				continue
			}
			
			key := parts[1]
			value, err := kvEngine.Get(key)
			if err != nil {
				conn.Write([]byte("$-1\r\n")) // Null bulk string
			} else {
				resp := fmt.Sprintf("$%d\r\n%s\r\n", len(value), value)
				conn.Write([]byte(resp))
			}
			
		case "SET":
			if len(parts) < 3 {
				conn.Write([]byte("-ERR wrong number of arguments\r\n"))
				continue
			}
			
			// Propose through Raft
			cmdStr := strings.Join(parts, " ")
			_, _, isLeader := raftNode.Propose([]byte(cmdStr))
			
			if !isLeader {
				conn.Write([]byte("-ERR not leader\r\n"))
			} else {
				// TODO: Wait for commit confirmation
				// For MVP, assume it will be committed
				time.Sleep(50 * time.Millisecond)
				conn.Write([]byte("+OK\r\n"))
			}
			
		case "DEL":
			if len(parts) < 2 {
				conn.Write([]byte("-ERR wrong number of arguments\r\n"))
				continue
			}
			
			// Propose through Raft
			cmdStr := strings.Join(parts, " ")
			_, _, isLeader := raftNode.Propose([]byte(cmdStr))
			
			if !isLeader {
				conn.Write([]byte("-ERR not leader\r\n"))
			} else {
				time.Sleep(50 * time.Millisecond)
				conn.Write([]byte(":1\r\n"))
			}
			
		case "RAFT_STATUS":
			term, isLeader := raftNode.GetState()
			status := "follower"
			if isLeader {
				status = "leader"
			}
			resp := fmt.Sprintf("+Node: %s, Term: %d, Status: %s\r\n", nodeID, term, status)
			conn.Write([]byte(resp))
			
		default:
			conn.Write([]byte("-ERR unknown command\r\n"))
		}
	}
}
