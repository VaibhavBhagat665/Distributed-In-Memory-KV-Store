package raft

import "time"

// NodeState represents the state of a Raft node
type NodeState int

const (
	Follower NodeState = iota
	Candidate
	Leader
)

func (s NodeState) String() string {
	switch s {
	case Follower:
		return "Follower"
	case Candidate:
		return "Candidate"
	case Leader:
		return "Leader"
	default:
		return "Unknown"
	}
}

// LogEntry represents a single entry in the replicated log
type LogEntry struct {
	Term    uint64 // Term when entry was received by leader
	Index   uint64 // Position in the log
	Command []byte // State machine command (serialized KV operation)
}

// AppendEntriesRequest is sent by leader to replicate log entries
type AppendEntriesRequest struct {
	Term         uint64     // Leader's term
	LeaderID     string     // So follower can redirect clients
	PrevLogIndex uint64     // Index of log entry immediately preceding new ones
	PrevLogTerm  uint64     // Term of prevLogIndex entry
	Entries      []LogEntry // Log entries to store (empty for heartbeat)
	LeaderCommit uint64     // Leader's commitIndex
}

// AppendEntriesResponse is the reply to AppendEntriesRequest
type AppendEntriesResponse struct {
	Term    uint64 // Current term, for leader to update itself
	Success bool   // True if follower contained entry matching prevLogIndex and prevLogTerm
	
	// Optimization: help leader find the next index to try
	ConflictIndex uint64 // Index of first entry with conflicting term
	ConflictTerm  uint64 // Term of conflicting entry
}

// RequestVoteRequest is sent by candidates to gather votes
type RequestVoteRequest struct {
	Term         uint64 // Candidate's term
	CandidateID  string // Candidate requesting vote
	LastLogIndex uint64 // Index of candidate's last log entry
	LastLogTerm  uint64 // Term of candidate's last log entry
}

// RequestVoteResponse is the reply to RequestVoteRequest
type RequestVoteResponse struct {
	Term        uint64 // Current term, for candidate to update itself
	VoteGranted bool   // True means candidate received vote
}

// Config holds Raft node configuration
type Config struct {
	ID                string        // Unique node identifier
	Peers             []string      // List of peer node IDs
	ElectionTimeout   time.Duration // Randomized timeout for elections
	HeartbeatInterval time.Duration // How often leader sends heartbeats
	
	// Storage for persistent state
	LogStore   LogStore   // Persistent log storage
	StateStore StateStore // Persistent state (term, votedFor)
}

// LogStore interface for persistent log storage
type LogStore interface {
	// Append entries to the log
	Append(entries []LogEntry) error
	
	// Get entry at index
	Get(index uint64) (*LogEntry, error)
	
	// Get entries in range [start, end]
	GetRange(start, end uint64) ([]LogEntry, error)
	
	// Get last log entry
	GetLast() (*LogEntry, error)
	
	// Delete entries from index onwards
	DeleteFrom(index uint64) error
	
	// Get log length
	Len() uint64
}

// StateStore interface for persistent state
type StateStore interface {
	// Get/Set current term
	GetTerm() (uint64, error)
	SetTerm(term uint64) error
	
	// Get/Set voted for in current term
	GetVotedFor() (string, error)
	SetVotedFor(candidateID string) error
}

// ApplyMsg is sent to the state machine when log entry is committed
type ApplyMsg struct {
	Index   uint64
	Command []byte
}
