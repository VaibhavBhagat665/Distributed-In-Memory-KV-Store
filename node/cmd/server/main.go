package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/yourusername/kvstore/engine"
	"github.com/yourusername/kvstore/server"
)

func main() {
	addr := flag.String("addr", ":6379", "Server address")
	threads := flag.Int("threads", 4, "Number of threads")
	memLimit := flag.Uint64("mem", 1024*1024*1024, "Memory limit in bytes")
	flag.Parse()
	
	// Create engine
	eng, err := engine.New(*threads, *memLimit)
	if err != nil {
		log.Fatalf("Failed to create engine: %v", err)
	}
	defer eng.Close()
	
	fmt.Printf("Starting KV store server...\n")
	fmt.Printf("  Address: %s\n", *addr)
	fmt.Printf("  Threads: %d\n", *threads)
	fmt.Printf("  Memory limit: %d MB\n", *memLimit/(1024*1024))
	
	// Create RESP server
	srv := server.NewRESPServer(*addr, eng)
	
	// Handle shutdown gracefully
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)
	
	go func() {
		<-sigChan
		fmt.Println("\nShutting down...")
		eng.Close()
		os.Exit(0)
	}()
	
	// Start server (blocks)
	if err := srv.Start(); err != nil {
		log.Fatalf("Server error: %v", err)
	}
}
