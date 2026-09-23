package raft

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"time"
)

// RPCServer handles incoming Raft RPC requests
type RPCServer struct {
	node *Node
	addr string
}

// NewRPCServer creates a new RPC server
func NewRPCServer(node *Node, addr string) *RPCServer {
	return &RPCServer{
		node: node,
		addr: addr,
	}
}

// Start begins listening for RPC requests
func (s *RPCServer) Start() error {
	mux := http.NewServeMux()
	mux.HandleFunc("/raft/request_vote", s.handleRequestVote)
	mux.HandleFunc("/raft/append_entries", s.handleAppendEntries)
	
	server := &http.Server{
		Addr:         s.addr,
		Handler:      mux,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 5 * time.Second,
	}
	
	fmt.Printf("[RPC] Listening on %s\n", s.addr)
	return server.ListenAndServe()
}

func (s *RPCServer) handleRequestVote(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	
	var req RequestVoteRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	
	resp := s.node.HandleRequestVote(&req)
	
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *RPCServer) handleAppendEntries(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	
	var req AppendEntriesRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	
	resp := s.node.HandleAppendEntries(&req)
	
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

// RPCClient sends RPC requests to peers
type RPCClient struct {
	client  *http.Client
	peerAddrs map[string]string // nodeID -> address
}

// NewRPCClient creates a new RPC client
func NewRPCClient(peerAddrs map[string]string) *RPCClient {
	return &RPCClient{
		client: &http.Client{
			Timeout: 2 * time.Second,
		},
		peerAddrs: peerAddrs,
	}
}

// SendRequestVote sends a RequestVote RPC to a peer
func (c *RPCClient) SendRequestVote(peerID string, req *RequestVoteRequest) (*RequestVoteResponse, error) {
	addr, ok := c.peerAddrs[peerID]
	if !ok {
		return nil, fmt.Errorf("unknown peer: %s", peerID)
	}
	
	url := fmt.Sprintf("http://%s/raft/request_vote", addr)
	
	body, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}
	
	httpResp, err := c.client.Post(url, "application/json", bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	defer httpResp.Body.Close()
	
	if httpResp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("RPC failed with status %d", httpResp.StatusCode)
	}
	
	var resp RequestVoteResponse
	if err := json.NewDecoder(httpResp.Body).Decode(&resp); err != nil {
		return nil, err
	}
	
	return &resp, nil
}

// SendAppendEntries sends an AppendEntries RPC to a peer
func (c *RPCClient) SendAppendEntries(peerID string, req *AppendEntriesRequest) (*AppendEntriesResponse, error) {
	addr, ok := c.peerAddrs[peerID]
	if !ok {
		return nil, fmt.Errorf("unknown peer: %s", peerID)
	}
	
	url := fmt.Sprintf("http://%s/raft/append_entries", addr)
	
	body, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}
	
	httpResp, err := c.client.Post(url, "application/json", bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	defer httpResp.Body.Close()
	
	if httpResp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("RPC failed with status %d", httpResp.StatusCode)
	}
	
	var resp AppendEntriesResponse
	if err := json.NewDecoder(httpResp.Body).Decode(&resp); err != nil {
		return nil, err
	}
	
	return &resp, nil
}
