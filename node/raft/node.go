package raft

import (
	"fmt"
	"math/rand"
	"sync"
	"time"
)

// Node represents a Raft consensus node
type Node struct {
	mu sync.Mutex
	
	// Configuration
	id    string
	peers []string
	
	// State
	state       NodeState
	currentTerm uint64
	votedFor    string
	
	// Log
	log         []LogEntry
	commitIndex uint64 // Index of highest log entry known to be committed
	lastApplied uint64 // Index of highest log entry applied to state machine
	
	// Leader state (reinitialized after election)
	nextIndex  map[string]uint64 // For each server, index of next log entry to send
	matchIndex map[string]uint64 // For each server, index of highest log entry known to be replicated
	
	// Channels
	applyCh chan ApplyMsg // Channel to send committed entries to state machine
	
	// Timers
	electionTimer  *time.Timer
	heartbeatTimer *time.Timer
	
	// Configuration
	config Config
	
	// RPC client
	rpcClient *RPCClient
	
	// Shutdown
	shutdown     chan struct{}
	shutdownOnce sync.Once
}

// NewNode creates a new Raft node
func NewNode(config Config, applyCh chan ApplyMsg, rpcClient *RPCClient) *Node {
	n := &Node{
		id:          config.ID,
		peers:       config.Peers,
		state:       Follower,
		currentTerm: 0,
		votedFor:    "",
		log:         make([]LogEntry, 0),
		commitIndex: 0,
		lastApplied: 0,
		nextIndex:   make(map[string]uint64),
		matchIndex:  make(map[string]uint64),
		applyCh:     applyCh,
		config:      config,
		rpcClient:   rpcClient,
		shutdown:    make(chan struct{}),
	}
	
	// Initialize timers
	n.resetElectionTimer()
	n.heartbeatTimer = time.NewTimer(config.HeartbeatInterval)
	n.heartbeatTimer.Stop() // Only active when leader
	
	// Start background routines
	go n.run()
	
	return n
}

// run is the main event loop
func (n *Node) run() {
	for {
		select {
		case <-n.shutdown:
			return
			
		case <-n.electionTimer.C:
			n.mu.Lock()
			n.startElection()
			n.mu.Unlock()
			
		case <-n.heartbeatTimer.C:
			n.mu.Lock()
			if n.state == Leader {
				n.sendHeartbeats()
				n.heartbeatTimer.Reset(n.config.HeartbeatInterval)
			}
			n.mu.Unlock()
		}
	}
}

// startElection initiates a new election
func (n *Node) startElection() {
	// Convert to candidate
	n.state = Candidate
	n.currentTerm++
	n.votedFor = n.id
	n.resetElectionTimer()
	
	fmt.Printf("[%s] Starting election for term %d\n", n.id, n.currentTerm)
	
	// Vote for self
	votesReceived := 1
	var voteMu sync.Mutex
	
	// Request votes from all peers
	for _, peerID := range n.peers {
		go func(peer string) {
			req := &RequestVoteRequest{
				Term:         n.currentTerm,
				CandidateID:  n.id,
				LastLogIndex: n.getLastLogIndex(),
				LastLogTerm:  n.getLastLogTerm(),
			}
			
			resp, err := n.sendRequestVote(peer, req)
			if err != nil {
				return
			}
			
			n.mu.Lock()
			defer n.mu.Unlock()
			
			// Check if we're still a candidate and in the same term
			if n.state != Candidate || n.currentTerm != req.Term {
				return
			}
			
			// If RPC response contains higher term, convert to follower
			if resp.Term > n.currentTerm {
				n.becomeFollower(resp.Term)
				return
			}
			
			// Count vote
			if resp.VoteGranted {
				voteMu.Lock()
				votesReceived++
				votes := votesReceived
				voteMu.Unlock()
				
				// Check if we have majority
				if votes > len(n.peers)/2+1 {
					n.becomeLeader()
				}
			}
		}(peerID)
	}
}

// becomeFollower transitions to follower state
func (n *Node) becomeFollower(term uint64) {
	n.state = Follower
	n.currentTerm = term
	n.votedFor = ""
	n.resetElectionTimer()
	n.heartbeatTimer.Stop()
	
	fmt.Printf("[%s] Became follower for term %d\n", n.id, term)
}

// becomeLeader transitions to leader state
func (n *Node) becomeLeader() {
	n.state = Leader
	n.heartbeatTimer.Stop()
	
	fmt.Printf("[%s] Became leader for term %d\n", n.id, n.currentTerm)
	
	// Initialize leader state
	lastLogIndex := n.getLastLogIndex()
	for _, peer := range n.peers {
		n.nextIndex[peer] = lastLogIndex + 1
		n.matchIndex[peer] = 0
	}
	
	// Send initial heartbeats
	n.sendHeartbeats()
	n.heartbeatTimer.Reset(n.config.HeartbeatInterval)
}

// sendHeartbeats sends empty AppendEntries to all peers
func (n *Node) sendHeartbeats() {
	for _, peerID := range n.peers {
		go n.sendAppendEntries(peerID)
	}
}

// sendAppendEntries sends log entries to a peer
func (n *Node) sendAppendEntries(peerID string) {
	n.mu.Lock()
	
	if n.state != Leader {
		n.mu.Unlock()
		return
	}
	
	nextIdx := n.nextIndex[peerID]
	prevLogIndex := nextIdx - 1
	prevLogTerm := uint64(0)
	
	if prevLogIndex > 0 && prevLogIndex <= uint64(len(n.log)) {
		prevLogTerm = n.log[prevLogIndex-1].Term
	}
	
	// Entries to send
	entries := []LogEntry{}
	if nextIdx <= uint64(len(n.log)) {
		entries = n.log[nextIdx-1:]
	}
	
	req := &AppendEntriesRequest{
		Term:         n.currentTerm,
		LeaderID:     n.id,
		PrevLogIndex: prevLogIndex,
		PrevLogTerm:  prevLogTerm,
		Entries:      entries,
		LeaderCommit: n.commitIndex,
	}
	
	n.mu.Unlock()
	
	resp, err := n.sendAppendEntriesRPC(peerID, req)
	if err != nil {
		return
	}
	
	n.mu.Lock()
	defer n.mu.Unlock()
	
	// Check if still leader and same term
	if n.state != Leader || n.currentTerm != req.Term {
		return
	}
	
	// If higher term, step down
	if resp.Term > n.currentTerm {
		n.becomeFollower(resp.Term)
		return
	}
	
	// Handle response
	if resp.Success {
		// Update nextIndex and matchIndex
		n.matchIndex[peerID] = prevLogIndex + uint64(len(entries))
		n.nextIndex[peerID] = n.matchIndex[peerID] + 1
		
		// Check if we can commit
		n.updateCommitIndex()
	} else {
		// Decrement nextIndex and retry
		if n.nextIndex[peerID] > 1 {
			n.nextIndex[peerID]--
		}
	}
}

// updateCommitIndex checks if we can advance commitIndex
func (n *Node) updateCommitIndex() {
	// Find highest N where majority of matchIndex[i] >= N
	for i := n.commitIndex + 1; i <= uint64(len(n.log)); i++ {
		if n.log[i-1].Term != n.currentTerm {
			continue
		}
		
		count := 1 // Count self
		for _, peer := range n.peers {
			if n.matchIndex[peer] >= i {
				count++
			}
		}
		
		if count > len(n.peers)/2+1 {
			n.commitIndex = i
			n.applyCommitted()
		}
	}
}

// applyCommitted applies committed entries to state machine
func (n *Node) applyCommitted() {
	for n.lastApplied < n.commitIndex {
		n.lastApplied++
		entry := n.log[n.lastApplied-1]
		
		msg := ApplyMsg{
			Index:   n.lastApplied,
			Command: entry.Command,
		}
		
		// Send to state machine (non-blocking)
		select {
		case n.applyCh <- msg:
		default:
			// Channel full, will retry later
		}
	}
}

// Propose submits a new command to the Raft log
func (n *Node) Propose(command []byte) (uint64, uint64, bool) {
	n.mu.Lock()
	defer n.mu.Unlock()
	
	if n.state != Leader {
		return 0, 0, false
	}
	
	// Append to log
	entry := LogEntry{
		Term:    n.currentTerm,
		Index:   uint64(len(n.log)) + 1,
		Command: command,
	}
	
	n.log = append(n.log, entry)
	
	fmt.Printf("[%s] Proposed entry at index %d\n", n.id, entry.Index)
	
	// Trigger replication
	go n.sendHeartbeats()
	
	return entry.Index, n.currentTerm, true
}

// Helper functions

func (n *Node) getLastLogIndex() uint64 {
	return uint64(len(n.log))
}

func (n *Node) getLastLogTerm() uint64 {
	if len(n.log) == 0 {
		return 0
	}
	return n.log[len(n.log)-1].Term
}

func (n *Node) resetElectionTimer() {
	timeout := n.config.ElectionTimeout + time.Duration(rand.Int63n(int64(n.config.ElectionTimeout)))
	if n.electionTimer == nil {
		n.electionTimer = time.NewTimer(timeout)
	} else {
		n.electionTimer.Stop()
		n.electionTimer.Reset(timeout)
	}
}

// Shutdown gracefully shuts down the node
func (n *Node) Shutdown() {
	n.shutdownOnce.Do(func() {
		close(n.shutdown)
		n.electionTimer.Stop()
		n.heartbeatTimer.Stop()
	})
}

// RPC methods

func (n *Node) sendRequestVote(peerID string, req *RequestVoteRequest) (*RequestVoteResponse, error) {
	return n.rpcClient.SendRequestVote(peerID, req)
}

func (n *Node) sendAppendEntriesRPC(peerID string, req *AppendEntriesRequest) (*AppendEntriesResponse, error) {
	return n.rpcClient.SendAppendEntries(peerID, req)
}

// HandleRequestVote processes incoming RequestVote RPC
func (n *Node) HandleRequestVote(req *RequestVoteRequest) *RequestVoteResponse {
	n.mu.Lock()
	defer n.mu.Unlock()
	
	resp := &RequestVoteResponse{
		Term:        n.currentTerm,
		VoteGranted: false,
	}
	
	// Reply false if term < currentTerm
	if req.Term < n.currentTerm {
		return resp
	}
	
	// If RPC request contains term > currentTerm, convert to follower
	if req.Term > n.currentTerm {
		n.becomeFollower(req.Term)
	}
	
	// Grant vote if:
	// 1. Haven't voted or already voted for this candidate
	// 2. Candidate's log is at least as up-to-date as ours
	if (n.votedFor == "" || n.votedFor == req.CandidateID) &&
		n.isLogUpToDate(req.LastLogIndex, req.LastLogTerm) {
		n.votedFor = req.CandidateID
		resp.VoteGranted = true
		n.resetElectionTimer()
	}
	
	resp.Term = n.currentTerm
	return resp
}

// HandleAppendEntries processes incoming AppendEntries RPC
func (n *Node) HandleAppendEntries(req *AppendEntriesRequest) *AppendEntriesResponse {
	n.mu.Lock()
	defer n.mu.Unlock()
	
	resp := &AppendEntriesResponse{
		Term:    n.currentTerm,
		Success: false,
	}
	
	// Reply false if term < currentTerm
	if req.Term < n.currentTerm {
		return resp
	}
	
	// If RPC request contains term >= currentTerm, convert to follower
	if req.Term >= n.currentTerm {
		n.becomeFollower(req.Term)
	}
	
	// Reset election timer (we heard from leader)
	n.resetElectionTimer()
	
	// Check if log contains entry at prevLogIndex with prevLogTerm
	if req.PrevLogIndex > 0 {
		if req.PrevLogIndex > uint64(len(n.log)) {
			resp.ConflictIndex = uint64(len(n.log)) + 1
			return resp
		}
		
		if n.log[req.PrevLogIndex-1].Term != req.PrevLogTerm {
			resp.ConflictTerm = n.log[req.PrevLogIndex-1].Term
			// Find first index of conflicting term
			for i := req.PrevLogIndex - 1; i >= 1; i-- {
				if n.log[i-1].Term != resp.ConflictTerm {
					resp.ConflictIndex = i + 1
					break
				}
			}
			return resp
		}
	}
	
	// Append new entries
	for i, entry := range req.Entries {
		idx := req.PrevLogIndex + uint64(i) + 1
		if idx <= uint64(len(n.log)) {
			// Existing entry: check for conflict
			if n.log[idx-1].Term != entry.Term {
				// Delete conflicting entry and all that follow
				n.log = n.log[:idx-1]
				n.log = append(n.log, entry)
			}
		} else {
			// New entry
			n.log = append(n.log, entry)
		}
	}
	
	// Update commitIndex
	if req.LeaderCommit > n.commitIndex {
		n.commitIndex = min(req.LeaderCommit, uint64(len(n.log)))
		n.applyCommitted()
	}
	
	resp.Success = true
	resp.Term = n.currentTerm
	return resp
}

// isLogUpToDate checks if candidate's log is at least as up-to-date as ours
func (n *Node) isLogUpToDate(lastIndex, lastTerm uint64) bool {
	ourLastTerm := n.getLastLogTerm()
	ourLastIndex := n.getLastLogIndex()
	
	if lastTerm != ourLastTerm {
		return lastTerm > ourLastTerm
	}
	return lastIndex >= ourLastIndex
}

func min(a, b uint64) uint64 {
	if a < b {
		return a
	}
	return b
}

// GetState returns current term and whether this node believes it's the leader
func (n *Node) GetState() (uint64, bool) {
	n.mu.Lock()
	defer n.mu.Unlock()
	return n.currentTerm, n.state == Leader
}
