#include "hash_table.h"
#include "wal.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>

using namespace kvstore;

// Forward declarations
void test_wal_write_read();
void test_wal_auto_batch();
void test_hash_table_wal_integration();
void test_wal_replay();
void test_partial_write_handling();
void test_operation_order_preservation();

void test_wal_write_read() {
    std::cout << "Test: WAL Write and Read..." << std::endl;
    
    const std::string wal_path = "/tmp/test_wal.log";
    
    // Clean up any existing file
    std::filesystem::remove(wal_path);
    
    // Write some records
    {
        WALWriter writer(wal_path, 2);  // batch_size = 2
        
        writer.append(WALRecord(WALOpType::SET, "key1", "value1", 0));
        writer.append(WALRecord(WALOpType::SET, "key2", "value2", 1000000000));
        writer.flush();  // Force flush
        
        writer.append(WALRecord(WALOpType::DELETE, "key1", "", 0));
        writer.flush();
    }
    
    // Read back and verify
    {
        WALReader reader(wal_path);
        WALRecord record(WALOpType::SET, "");
        
        // Record 1: SET key1=value1
        assert(reader.read_next(record));
        assert(record.op_type == WALOpType::SET);
        assert(record.key == "key1");
        assert(record.value == "value1");
        assert(record.ttl_ns == 0);
        
        // Record 2: SET key2=value2
        assert(reader.read_next(record));
        assert(record.op_type == WALOpType::SET);
        assert(record.key == "key2");
        assert(record.value == "value2");
        assert(record.ttl_ns == 1000000000);
        
        // Record 3: DELETE key1
        assert(reader.read_next(record));
        assert(record.op_type == WALOpType::DELETE);
        assert(record.key == "key1");
        assert(record.value.empty());
        
        // No more records
        assert(!reader.read_next(record));
        
        assert(reader.records_read() == 3);
    }
    
    // Clean up
    std::filesystem::remove(wal_path);
    
    std::cout << "  PASSED" << std::endl;
}

void test_wal_auto_batch() {
    std::cout << "Test: WAL Auto-Batching..." << std::endl;
    
    const std::string wal_path = "/tmp/test_wal_batch.log";
    std::filesystem::remove(wal_path);
    
    {
        WALWriter writer(wal_path, 3);  // Auto-flush every 3 records
        
        writer.append(WALRecord(WALOpType::SET, "k1", "v1"));
        writer.append(WALRecord(WALOpType::SET, "k2", "v2"));
        // Batch not full yet, pending = 2
        assert(writer.pending_count() == 2);
        
        writer.append(WALRecord(WALOpType::SET, "k3", "v3"));
        // Should auto-flush, pending = 0
        assert(writer.pending_count() == 0);
        
        writer.append(WALRecord(WALOpType::SET, "k4", "v4"));
        assert(writer.pending_count() == 1);
    } // Destructor flushes remaining
    
    // Verify all 4 records written
    WALReader reader(wal_path);
    WALRecord record(WALOpType::SET, "");
    int count = 0;
    while (reader.read_next(record)) {
        count++;
    }
    assert(count == 4);
    
    std::filesystem::remove(wal_path);
    std::cout << "  PASSED" << std::endl;
}

void test_hash_table_wal_integration() {
    std::cout << "Test: HashTable WAL Integration..." << std::endl;
    
    const std::string wal_path = "/tmp/test_hashtable_wal.log";
    std::filesystem::remove(wal_path);
    
    // Create hash table with WAL enabled
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU, wal_path);
        
        ht.insert("apple", ValueEntry("red", 0));
        ht.insert("banana", ValueEntry("yellow", 0));
        ht.insert("cherry", ValueEntry("red", 0));
        
        ht.flush_wal();
        
        ht.remove("banana");
        ht.flush_wal();
        
        assert(ht.size() == 2);
    }
    
    // Verify WAL contains 4 operations
    WALReader reader(wal_path);
    assert(reader.records_read() == 0);
    
    WALRecord record(WALOpType::SET, "");
    int ops = 0;
    while (reader.read_next(record)) {
        ops++;
    }
    assert(ops == 4);  // 3 SETs + 1 DELETE
    
    std::filesystem::remove(wal_path);
    std::cout << "  PASSED" << std::endl;
}

void test_wal_replay() {
    std::cout << "Test: WAL Replay Recovery..." << std::endl;
    
    const std::string wal_path = "/tmp/test_replay.log";
    std::filesystem::remove(wal_path);
    
    // Create initial state
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU, wal_path);
        
        ht.insert("user:1", ValueEntry("Alice", 0));
        ht.insert("user:2", ValueEntry("Bob", 0));
        ht.insert("user:3", ValueEntry("Charlie", 0));
        ht.remove("user:2");
        ht.insert("user:4", ValueEntry("Diana", 0));
        
        ht.flush_wal();
        
        // Verify state before "crash"
        assert(ht.size() == 3);
        auto val = ht.get("user:1");
        assert(val && (*val)->value == "Alice");
        val = ht.get("user:2");
        assert(!val);  // Deleted
        val = ht.get("user:4");
        assert(val && (*val)->value == "Diana");
    }
    
    // Simulate crash: create new hash table and replay WAL
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        
        size_t replayed = ht.replay_wal(wal_path);
        assert(replayed == 5);  // 4 SETs + 1 DELETE
        
        // Verify recovered state
        assert(ht.size() == 3);
        
        auto val = ht.get("user:1");
        assert(val && (*val)->value == "Alice");
        
        val = ht.get("user:2");
        assert(!val);  // Should still be deleted
        
        val = ht.get("user:3");
        assert(val && (*val)->value == "Charlie");
        
        val = ht.get("user:4");
        assert(val && (*val)->value == "Diana");
    }
    
    std::filesystem::remove(wal_path);
    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "==================" << std::endl;
    std::cout << "WAL Tests" << std::endl;
    std::cout << "==================" << std::endl;
    
    test_wal_write_read();
    test_wal_auto_batch();
    test_hash_table_wal_integration();
    test_wal_replay();
    test_partial_write_handling();
    test_operation_order_preservation();
    
    std::cout << "\nAll WAL tests passed! (6/6)" << std::endl;
    return 0;
}


void test_partial_write_handling() {
    std::cout << "Test: Partial Write Handling..." << std::endl;
    
    const std::string wal_path = "/tmp/test_partial.log";
    std::filesystem::remove(wal_path);
    
    // Write 2 complete records + 1 partial record
    {
        WALWriter writer(wal_path, 10);  // Large batch to control flushing
        
        writer.append(WALRecord(WALOpType::SET, "complete1", "value1", 0));
        writer.append(WALRecord(WALOpType::SET, "complete2", "value2", 0));
        writer.flush();
        
        // Manually append incomplete data (simulate crash mid-write)
        std::ofstream file(wal_path, std::ios::binary | std::ios::app);
        uint8_t op = static_cast<uint8_t>(WALOpType::SET);
        file.write(reinterpret_cast<const char*>(&op), 1);
        uint32_t key_len = 5;
        file.write(reinterpret_cast<const char*>(&key_len), 4);
        // Write only 3 bytes of key (incomplete)
        file.write("par", 3);
        file.close();
    }
    
    // Reader should gracefully handle partial record
    {
        WALReader reader(wal_path);
        WALRecord record(WALOpType::SET, "");
        
        // Read first complete record
        assert(reader.read_next(record));
        assert(record.key == "complete1");
        
        // Read second complete record
        assert(reader.read_next(record));
        assert(record.key == "complete2");
        
        // Attempt to read partial record - should return false (EOF/incomplete)
        assert(!reader.read_next(record));
        
        // Should have read exactly 2 complete records
        assert(reader.records_read() == 2);
    }
    
    // HashTable replay should handle partial writes gracefully
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        size_t replayed = ht.replay_wal(wal_path);
        
        // Should replay only the 2 complete records
        assert(replayed == 2);
        assert(ht.size() == 2);
        
        auto val = ht.get("complete1");
        assert(val && (*val)->value == "value1");
        
        val = ht.get("complete2");
        assert(val && (*val)->value == "value2");
    }
    
    std::filesystem::remove(wal_path);
    std::cout << "  PASSED" << std::endl;
}

void test_operation_order_preservation() {
    std::cout << "Test: Operation Order Preservation..." << std::endl;
    
    const std::string wal_path = "/tmp/test_order.log";
    std::filesystem::remove(wal_path);
    
    // Create sequence of operations with specific order
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU, wal_path);
        
        // Order matters here: SET → UPDATE → DELETE → SET different key
        ht.insert("counter", ValueEntry("1", 0));
        ht.insert("counter", ValueEntry("2", 0));  // Update
        ht.insert("counter", ValueEntry("3", 0));  // Update again
        ht.remove("counter");                       // Delete
        ht.insert("new_key", ValueEntry("data", 0)); // Different key
        
        ht.flush_wal();
        
        // Verify final state before replay
        assert(ht.size() == 1);
        auto val = ht.get("counter");
        assert(!val);  // Should be deleted
        val = ht.get("new_key");
        assert(val && (*val)->value == "data");
    }
    
    // Replay and verify same final state
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        size_t replayed = ht.replay_wal(wal_path);
        
        assert(replayed == 5);  // 4 SETs + 1 DELETE
        
        // Final state should match: counter deleted, new_key exists
        assert(ht.size() == 1);
        
        auto val = ht.get("counter");
        assert(!val);  // Should still be deleted after replay
        
        val = ht.get("new_key");
        assert(val && (*val)->value == "data");
    }
    
    std::filesystem::remove(wal_path);
    std::cout << "  PASSED" << std::endl;
}
