package raft

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sync"
)

// MemoryLogStore is an in-memory implementation of LogStore (for testing/MVP)
type MemoryLogStore struct {
	mu      sync.RWMutex
	entries []LogEntry
}

func NewMemoryLogStore() *MemoryLogStore {
	return &MemoryLogStore{
		entries: make([]LogEntry, 0),
	}
}

func (m *MemoryLogStore) Append(entries []LogEntry) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.entries = append(m.entries, entries...)
	return nil
}

func (m *MemoryLogStore) Get(index uint64) (*LogEntry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if index == 0 || index > uint64(len(m.entries)) {
		return nil, fmt.Errorf("index out of range")
	}
	
	entry := m.entries[index-1]
	return &entry, nil
}

func (m *MemoryLogStore) GetRange(start, end uint64) ([]LogEntry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if start == 0 || start > uint64(len(m.entries)) || end > uint64(len(m.entries)) {
		return nil, fmt.Errorf("range out of bounds")
	}
	
	entries := make([]LogEntry, end-start+1)
	copy(entries, m.entries[start-1:end])
	return entries, nil
}

func (m *MemoryLogStore) GetLast() (*LogEntry, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if len(m.entries) == 0 {
		return nil, fmt.Errorf("log is empty")
	}
	
	entry := m.entries[len(m.entries)-1]
	return &entry, nil
}

func (m *MemoryLogStore) DeleteFrom(index uint64) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if index == 0 {
		return fmt.Errorf("invalid index")
	}
	
	if index <= uint64(len(m.entries)) {
		m.entries = m.entries[:index-1]
	}
	
	return nil
}

func (m *MemoryLogStore) Len() uint64 {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return uint64(len(m.entries))
}

// FileStateStore persists Raft state to disk
type FileStateStore struct {
	mu       sync.RWMutex
	dir      string
	term     uint64
	votedFor string
}

func NewFileStateStore(dir string) (*FileStateStore, error) {
	if err := os.MkdirAll(dir, 0755); err != nil {
		return nil, err
	}
	
	store := &FileStateStore{
		dir:      dir,
		term:     0,
		votedFor: "",
	}
	
	// Load existing state if present
	if err := store.load(); err != nil && !os.IsNotExist(err) {
		return nil, err
	}
	
	return store, nil
}

func (f *FileStateStore) GetTerm() (uint64, error) {
	f.mu.RLock()
	defer f.mu.RUnlock()
	return f.term, nil
}

func (f *FileStateStore) SetTerm(term uint64) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.term = term
	return f.persist()
}

func (f *FileStateStore) GetVotedFor() (string, error) {
	f.mu.RLock()
	defer f.mu.RUnlock()
	return f.votedFor, nil
}

func (f *FileStateStore) SetVotedFor(candidateID string) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.votedFor = candidateID
	return f.persist()
}

type stateData struct {
	Term     uint64 `json:"term"`
	VotedFor string `json:"voted_for"`
}

func (f *FileStateStore) persist() error {
	data := stateData{
		Term:     f.term,
		VotedFor: f.votedFor,
	}
	
	bytes, err := json.Marshal(data)
	if err != nil {
		return err
	}
	
	path := filepath.Join(f.dir, "raft_state.json")
	tmpPath := path + ".tmp"
	
	// Write to temp file first
	if err := os.WriteFile(tmpPath, bytes, 0644); err != nil {
		return err
	}
	
	// Atomic rename
	return os.Rename(tmpPath, path)
}

func (f *FileStateStore) load() error {
	path := filepath.Join(f.dir, "raft_state.json")
	
	bytes, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	
	var data stateData
	if err := json.Unmarshal(bytes, &data); err != nil {
		return err
	}
	
	f.term = data.Term
	f.votedFor = data.VotedFor
	
	return nil
}
