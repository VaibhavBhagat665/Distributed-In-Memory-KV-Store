package server

import (
	"bufio"
	"fmt"
	"io"
	"net"
	"strconv"
	"strings"
	"time"

	"github.com/yourusername/kvstore/engine"
)

// RESPServer is a simple RESP-compatible server
type RESPServer struct {
	addr   string
	engine *engine.Engine
}

// NewRESPServer creates a new RESP server
func NewRESPServer(addr string, eng *engine.Engine) *RESPServer {
	return &RESPServer{
		addr:   addr,
		engine: eng,
	}
}

// Start starts the RESP server
func (s *RESPServer) Start() error {
	listener, err := net.Listen("tcp", s.addr)
	if err != nil {
		return fmt.Errorf("failed to listen on %s: %w", s.addr, err)
	}
	defer listener.Close()
	
	fmt.Printf("RESP server listening on %s\n", s.addr)
	
	for {
		conn, err := listener.Accept()
		if err != nil {
			fmt.Printf("Accept error: %v\n", err)
			continue
		}
		
		go s.handleConnection(conn)
	}
}

func (s *RESPServer) handleConnection(conn net.Conn) {
	defer conn.Close()
	
	reader := bufio.NewReader(conn)
	
	for {
		// Read RESP command
		cmd, err := s.readRESPArray(reader)
		if err != nil {
			if err != io.EOF {
				fmt.Printf("Read error: %v\n", err)
			}
			return
		}
		
		// Handle command
		response := s.handleCommand(cmd)
		
		// Write response
		if _, err := conn.Write([]byte(response)); err != nil {
			fmt.Printf("Write error: %v\n", err)
			return
		}
	}
}

func (s *RESPServer) readRESPArray(reader *bufio.Reader) ([]string, error) {
	// Read array length line: *<count>\r\n
	line, err := reader.ReadString('\n')
	if err != nil {
		return nil, err
	}
	
	line = strings.TrimSpace(line)
	if len(line) == 0 || line[0] != '*' {
		return nil, fmt.Errorf("expected array, got: %s", line)
	}
	
	count, err := strconv.Atoi(line[1:])
	if err != nil {
		return nil, fmt.Errorf("invalid array count: %s", line)
	}
	
	result := make([]string, count)
	for i := 0; i < count; i++ {
		// Read bulk string: $<length>\r\n<data>\r\n
		lengthLine, err := reader.ReadString('\n')
		if err != nil {
			return nil, err
		}
		
		lengthLine = strings.TrimSpace(lengthLine)
		if len(lengthLine) == 0 || lengthLine[0] != '$' {
			return nil, fmt.Errorf("expected bulk string, got: %s", lengthLine)
		}
		
		length, err := strconv.Atoi(lengthLine[1:])
		if err != nil {
			return nil, fmt.Errorf("invalid bulk string length: %s", lengthLine)
		}
		
		// Read data + \r\n
		data := make([]byte, length+2)
		if _, err := io.ReadFull(reader, data); err != nil {
			return nil, err
		}
		
		result[i] = string(data[:length])
	}
	
	return result, nil
}

func (s *RESPServer) handleCommand(cmd []string) string {
	if len(cmd) == 0 {
		return "-ERR empty command\r\n"
	}
	
	command := strings.ToUpper(cmd[0])
	
	switch command {
	case "PING":
		return "+PONG\r\n"
		
	case "SET":
		if len(cmd) < 3 {
			return "-ERR wrong number of arguments for 'set'\r\n"
		}
		key, value := cmd[1], cmd[2]
		
		// Parse optional TTL (EX seconds or PX milliseconds)
		var ttl time.Duration
		for i := 3; i < len(cmd)-1; i++ {
			switch strings.ToUpper(cmd[i]) {
			case "EX":
				seconds, err := strconv.Atoi(cmd[i+1])
				if err == nil {
					ttl = time.Duration(seconds) * time.Second
				}
				i++
			case "PX":
				ms, err := strconv.Atoi(cmd[i+1])
				if err == nil {
					ttl = time.Duration(ms) * time.Millisecond
				}
				i++
			}
		}
		
		if err := s.engine.Set(key, value, ttl); err != nil {
			return fmt.Sprintf("-ERR %v\r\n", err)
		}
		return "+OK\r\n"
		
	case "GET":
		if len(cmd) != 2 {
			return "-ERR wrong number of arguments for 'get'\r\n"
		}
		key := cmd[1]
		
		value, err := s.engine.Get(key)
		if err == engine.ErrNotFound {
			return "$-1\r\n" // Null bulk string
		}
		if err != nil {
			return fmt.Sprintf("-ERR %v\r\n", err)
		}
		
		return fmt.Sprintf("$%d\r\n%s\r\n", len(value), value)
		
	case "DEL":
		if len(cmd) != 2 {
			return "-ERR wrong number of arguments for 'del'\r\n"
		}
		key := cmd[1]
		
		if err := s.engine.Delete(key); err != nil && err != engine.ErrNotFound {
			return fmt.Sprintf("-ERR %v\r\n", err)
		}
		return ":1\r\n"
		
	default:
		return fmt.Sprintf("-ERR unknown command '%s'\r\n", command)
	}
}
