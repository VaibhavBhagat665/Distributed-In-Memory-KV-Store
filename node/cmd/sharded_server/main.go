package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"net"
	"strconv"
	"strings"
	"time"

	"github.com/VaibhavBhagat665/Distributed-In-Memory-KV-Store/engine"
	"github.com/VaibhavBhagat665/Distributed-In-Memory-KV-Store/raft"
	"github.com/VaibhavBhagat665/Distributed-In-Memory-KV-Store/sharding"
)

// ShardedKVServer manages a sharded KV store with multi-raft
type ShardedKVServer struct {
	nodeID      int
	port        int
	numShards   int
	router      *sharding.ShardRouter
	multiRaft   *sharding.MultiRaftManager
	engines     map[sharding.ShardID]*engine.Engine
	listener    net.Listener
}

func main() {
	// Command-line flags
	nodeID := flag.Int("node-id", 0, "Node ID (0, 1, or 2)")
	port := flag.Int("port", 7000, "Client port")
	numShards := flag.Int("shards", 3, "Number of shards")
	
	flag.Parse()

	log.Printf("Starting sharded KV server: node=%d, port=%d, shards=%d", 
		*nodeID, *port, *numShards)

	// Create server
	server := NewShardedKVServer(*nodeID, *port, *numShards)

	// Start server
	if err := server.Start(); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}

	// Block forever
	select {}
}

// NewShardedKVServer creates a new sharded KV server
func NewShardedKVServer(nodeID, port, numShards int) *ShardedKVServer {
	return &ShardedKVServer{
		nodeID:    nodeID,
		port:      port,
		numShards: numShards,
		engines:   make(map[sharding.ShardID]*engine.Engine),
	}
}

// Start initializes and starts the sharded server
func (sks *ShardedKVServer) Start() error {
	// Step 1: Initialize multi-raft manager
	config := sharding.MultiRaftConfig{
		NumShards:         sks.numShards,
		NodesPerShard:     3,
		BaseRaftPort:      9000,
		BaseClientPort:    6379,
		HeartbeatInterval: 50 * time.Millisecond,
		ElectionTimeout:   150 * time.Millisecond,
		NodeID:            sks.nodeID,
	}
	sks.multiRaft = sharding.NewMultiRaftManager(config)

	// Step 2: Start each shard
	shardIDs := make([]sharding.ShardID, sks.numShards)
	for i := 0; i < sks.numShards; i++ {
		shardID := sharding.ShardID(i)
		shardIDs[i] = shardID

		// Create peer list for this shard
		peers := make([]string, 3)
		for j := 0; j < 3; j++ {
			raftPort := config.BaseRaftPort + (i * 100) + j
			peers[j] = fmt.Sprintf("localhost:%d", raftPort)
		}

		// Start shard
		if err := sks.multiRaft.StartShard(shardID, peers); err != nil {
			return fmt.Errorf("failed to start shard %d: %w", shardID, err)
		}

		// Create KV engine for this shard
		sks.engines[shardID] = engine.New()

		// Start apply loop for this shard
		go sks.applyLoop(shardID)

		log.Printf("Started shard %d with peers: %v", shardID, peers)
	}

	// Step 3: Initialize hash ring
	hashRing := sharding.NewHashRing(shardIDs, 150)

	// Step 4: Initialize router
	routerConfig := sharding.RouterConfig{
		HashRing:   hashRing,
		MultiRaft:  sks.multiRaft,
		MaxRetries: 3,
		RetryDelay: 50 * time.Millisecond,
	}
	sks.router = sharding.NewShardRouter(routerConfig)

	// Step 5: Start RESP server
	return sks.startRESPServer()
}

// startRESPServer starts the RESP protocol server for client connections
func (sks *ShardedKVServer) startRESPServer() error {
	listener, err := net.Listen("tcp", fmt.Sprintf(":%d", sks.port))
	if err != nil {
		return fmt.Errorf("failed to listen: %w", err)
	}
	sks.listener = listener

	log.Printf("Sharded KV server listening on port %d", sks.port)

	go func() {
		for {
			conn, err := listener.Accept()
			if err != nil {
				log.Printf("Accept error: %v", err)
				continue
			}
			go sks.handleConnection(conn)
		}
	}()

	return nil
}

// handleConnection handles a single client connection
func (sks *ShardedKVServer) handleConnection(conn net.Conn) {
	defer conn.Close()
	scanner := bufio.NewScanner(conn)

	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.Fields(line)

		if len(parts) == 0 {
			continue
		}

		command := strings.ToUpper(parts[0])

		switch command {
		case "SET":
			sks.handleSET(conn, parts)
		case "GET":
			sks.handleGET(conn, parts)
		case "DEL":
			sks.handleDEL(conn, parts)
		case "STATUS":
			sks.handleSTATUS(conn)
		case "PING":
			conn.Write([]byte("+PONG\r\n"))
		default:
			conn.Write([]byte("-ERR unknown command\r\n"))
		}
	}
}

// handleSET handles SET command
func (sks *ShardedKVServer) handleSET(conn net.Conn, parts []string) {
	if len(parts) < 3 {
		conn.Write([]byte("-ERR wrong number of arguments\r\n"))
		return
	}

	key := parts[1]
	value := parts[2]
	ttl := uint64(0)

	if len(parts) >= 4 {
		if t, err := strconv.ParseUint(parts[3], 10, 64); err == nil {
			ttl = t
		}
	}

	// Route to correct shard
	op := sharding.Operation{
		Type:  "SET",
		Key:   key,
		Value: value,
		TTL:   ttl,
	}

	_, err := sks.router.Route(op)
	if err != nil {
		conn.Write([]byte(fmt.Sprintf("-ERR %v\r\n", err)))
		return
	}

	// For MVP, assume success
	conn.Write([]byte("+OK\r\n"))
}

// handleGET handles GET command
func (sks *ShardedKVServer) handleGET(conn net.Conn, parts []string) {
	if len(parts) < 2 {
		conn.Write([]byte("-ERR wrong number of arguments\r\n"))
		return
	}

	key := parts[1]
	shardID := sks.router.GetShardForKey(key)

	// For MVP, read directly from local engine (eventual consistency)
	eng, exists := sks.engines[shardID]
	if !exists {
		conn.Write([]byte("-ERR shard not found\r\n"))
		return
	}

	value := eng.Get(key)
	if value == "" {
		conn.Write([]byte("$-1\r\n")) // Null bulk string
		return
	}

	// RESP bulk string
	conn.Write([]byte(fmt.Sprintf("$%d\r\n%s\r\n", len(value), value)))
}

// handleDEL handles DEL command
func (sks *ShardedKVServer) handleDEL(conn net.Conn, parts []string) {
	if len(parts) < 2 {
		conn.Write([]byte("-ERR wrong number of arguments\r\n"))
		return
	}

	key := parts[1]

	// Route to correct shard
	op := sharding.Operation{
		Type: "DEL",
		Key:  key,
	}

	_, err := sks.router.Route(op)
	if err != nil {
		conn.Write([]byte(fmt.Sprintf("-ERR %v\r\n", err)))
		return
	}

	conn.Write([]byte(":1\r\n")) // Deleted 1 key
}

// handleSTATUS handles STATUS command (shows shard info)
func (sks *ShardedKVServer) handleSTATUS(conn net.Conn) {
	statuses := sks.router.GetShardStatus()

	response := "Shard Status:\n"
	for shardID, status := range statuses {
		response += fmt.Sprintf("  Shard %d: Leader=%s, Term=%d, State=%s, CommitIndex=%d\n",
			shardID, status.Leader, status.Term, status.State, status.CommitIndex)
	}

	conn.Write([]byte(fmt.Sprintf("+%s\r\n", response)))
}

// applyLoop applies committed entries to the KV engine for a shard
func (sks *ShardedKVServer) applyLoop(shardID sharding.ShardID) {
	applyCh, err := sks.multiRaft.GetApplyChannel(shardID)
	if err != nil {
		log.Printf("Failed to get apply channel for shard %d: %v", shardID, err)
		return
	}

	eng := sks.engines[shardID]

	for msg := range applyCh {
		if !msg.CommandValid {
			continue
		}

		// Parse command
		command := string(msg.Command)
		parts := strings.Fields(command)

		if len(parts) == 0 {
			continue
		}

		cmd := parts[0]

		switch cmd {
		case "SET":
			if len(parts) >= 3 {
				key := parts[1]
				value := parts[2]
				ttl := time.Duration(0)

				if len(parts) >= 4 {
					if t, err := strconv.ParseUint(parts[3], 10, 64); err == nil {
						ttl = time.Duration(t) * time.Second
					}
				}

				eng.Set(key, value, ttl)
				log.Printf("[Shard %d] Applied: SET %s = %s (ttl=%v)", shardID, key, value, ttl)
			}

		case "DEL":
			if len(parts) >= 2 {
				key := parts[1]
				eng.Delete(key)
				log.Printf("[Shard %d] Applied: DEL %s", shardID, key)
			}
		}
	}
}
