package engine

import (
	"testing"
	"time"
)

func TestEngineBasicOperations(t *testing.T) {
	e, err := New(4, 100*1024*1024) // 4 threads, 100MB
	if err != nil {
		t.Fatalf("Failed to create engine: %v", err)
	}
	defer e.Close()
	
	// Test SET
	if err := e.Set("key1", "value1", 0); err != nil {
		t.Fatalf("Set failed: %v", err)
	}
	
	// Test GET
	val, err := e.Get("key1")
	if err != nil {
		t.Fatalf("Get failed: %v", err)
	}
	if val != "value1" {
		t.Errorf("Expected 'value1', got '%s'", val)
	}
	
	// Test UPDATE
	if err := e.Set("key1", "value2", 0); err != nil {
		t.Fatalf("Update failed: %v", err)
	}
	
	val, err = e.Get("key1")
	if err != nil {
		t.Fatalf("Get after update failed: %v", err)
	}
	if val != "value2" {
		t.Errorf("Expected 'value2', got '%s'", val)
	}
	
	// Test DELETE
	if err := e.Delete("key1"); err != nil {
		t.Fatalf("Delete failed: %v", err)
	}
	
	// Verify deleted
	_, err = e.Get("key1")
	if err != ErrNotFound {
		t.Errorf("Expected ErrNotFound, got %v", err)
	}
}

func TestEngineNotFound(t *testing.T) {
	e, err := New(4, 100*1024*1024)
	if err != nil {
		t.Fatalf("Failed to create engine: %v", err)
	}
	defer e.Close()
	
	_, err = e.Get("nonexistent")
	if err != ErrNotFound {
		t.Errorf("Expected ErrNotFound, got %v", err)
	}
}

func TestEngineTTL(t *testing.T) {
	e, err := New(4, 100*1024*1024)
	if err != nil {
		t.Fatalf("Failed to create engine: %v", err)
	}
	defer e.Close()
	
	// Set with 100ms TTL
	if err := e.Set("ttl_key", "ttl_value", 100*time.Millisecond); err != nil {
		t.Fatalf("Set with TTL failed: %v", err)
	}
	
	// Should be retrievable immediately
	val, err := e.Get("ttl_key")
	if err != nil {
		t.Fatalf("Get before expiry failed: %v", err)
	}
	if val != "ttl_value" {
		t.Errorf("Expected 'ttl_value', got '%s'", val)
	}
	
	// Wait for expiration
	time.Sleep(150 * time.Millisecond)
	
	// Should be expired
	_, err = e.Get("ttl_key")
	if err != ErrNotFound {
		t.Errorf("Expected ErrNotFound after TTL, got %v", err)
	}
}

func TestEngineMultipleKeys(t *testing.T) {
	e, err := New(4, 100*1024*1024)
	if err != nil {
		t.Fatalf("Failed to create engine: %v", err)
	}
	defer e.Close()
	
	// Insert multiple keys
	for i := 0; i < 100; i++ {
		key := string(rune('a' + (i % 26)))
		val := string(rune('A' + (i % 26)))
		if err := e.Set(key, val, 0); err != nil {
			t.Fatalf("Set key %d failed: %v", i, err)
		}
	}
	
	// Verify all keys
	for i := 0; i < 100; i++ {
		key := string(rune('a' + (i % 26)))
		expected := string(rune('A' + (i % 26)))
		
		val, err := e.Get(key)
		if err != nil {
			t.Fatalf("Get key %d failed: %v", i, err)
		}
		if val != expected {
			t.Errorf("Key %s: expected '%s', got '%s'", key, expected, val)
		}
	}
}
