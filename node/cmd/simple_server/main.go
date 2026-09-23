package main

import (
	"bufio"
	"flag"
	"fmt"
	"net"
	"os"
	"strings"
	"time"

	"github.com/yourusername/kvstore/engine"
)

var (
	port    = flag.Int("port", 6379, "Server port")
	threads = flag.Int("threads", 4, "Number of threads")
	memory  = flag.Uint64("memory", 1024*1024*1024, "Memory limit")
)

func main() {
	flag.Parse()

	fmt.Printf("=== Simple KV Server (No Raft) ===\n")
	fmt.Printf("Port: %d\n", *port)
	fmt.Printf("Threads: %d\n", *threads)
	fmt.Println()

	// Create engine
	kvEngine, err := engine.New(*threads, *memory)
	if err != nil {
		fmt.Printf("Failed to create engine: %v\n", err)
		os.Exit(1)
	}
	defer kvEngine.Close()

	// Start server
	listener, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		fmt.Printf("Failed to listen: %v\n", err)
		os.Exit(1)
	}
	defer listener.Close()

	fmt.Printf("Server ready on port %d\n\n", *port)

	for {
		conn, err := listener.Accept()
		if err != nil {
			continue
		}
		go handleClient(conn, kvEngine)
	}
}

func handleClient(conn net.Conn, kvEngine *engine.Engine) {
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

		case "SET":
			if len(parts) < 3 {
				conn.Write([]byte("-ERR wrong number of arguments\r\n"))
				continue
			}

			key := parts[1]
			value := parts[2]
			ttl := time.Duration(0)

			if len(parts) >= 5 && strings.ToUpper(parts[3]) == "EX" {
				var seconds int
				fmt.Sscanf(parts[4], "%d", &seconds)
				ttl = time.Duration(seconds) * time.Second
			}

			err := kvEngine.Set(key, value, ttl)
			if err != nil {
				conn.Write([]byte(fmt.Sprintf("-ERR %v\r\n", err)))
			} else {
				fmt.Printf("SET %s = %s\n", key, value)
				conn.Write([]byte("+OK\r\n"))
			}

		case "GET":
			if len(parts) < 2 {
				conn.Write([]byte("-ERR wrong number of arguments\r\n"))
				continue
			}

			key := parts[1]
			value, err := kvEngine.Get(key)
			if err != nil {
				conn.Write([]byte("$-1\r\n"))
			} else {
				resp := fmt.Sprintf("$%d\r\n%s\r\n", len(value), value)
				conn.Write([]byte(resp))
			}

		case "DEL":
			if len(parts) < 2 {
				conn.Write([]byte("-ERR wrong number of arguments\r\n"))
				continue
			}

			key := parts[1]
			err := kvEngine.Delete(key)
			if err != nil {
				conn.Write([]byte(":0\r\n"))
			} else {
				fmt.Printf("DEL %s\n", key)
				conn.Write([]byte(":1\r\n"))
			}

		default:
			conn.Write([]byte("-ERR unknown command\r\n"))
		}
	}
}
