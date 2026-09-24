#include "hash_table.h"
#include "snapshot.h"
#include <cassert>
#include <iostream>
#include <filesystem>

using namespace kvstore;

void test_snapshot_create_load() {
    std::cout << "Test: Snapshot Create and Load..." << std::endl;
    
    const std::string snapshot_path = "/tmp/test_snapshot.snap";
    std::filesystem::remove(snapshot_path);
    
    // Create hash table with data
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        
        ht.insert("user:1", ValueEntry("Alice", 0));
        ht.insert("user:2", ValueEntry("Bob", 1000000000));  // with TTL
        ht.insert("user:3", ValueEntry("Charlie", 0));
        ht.insert("config:timeout", ValueEntry("30", 0));
        
        assert(ht.size() == 4);
        
        // Create snapshot
        bool success = ht.create_snapshot(snapshot_path);
        assert(success);
        assert(std::filesystem::exists(snapshot_path));
    }
    
    // Load snapshot into new hash table
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        assert(ht.size() == 0);
        
        bool success = ht.load_snapshot(snapshot_path);
        assert(success);
        assert(ht.size() == 4);
        
        // Verify data
        auto val = ht.get("user:1");
        assert(val && (*val)->value == "Alice");
        
        val = ht.get("user:2");
        assert(val && (*val)->value == "Bob");
        assert((*val)->expiry_ns == 1000000000);
        
        val = ht.get("user:3");
        assert(val && (*val)->value == "Charlie");
        
        val = ht.get("config:timeout");
        assert(val && (*val)->value == "30");
    }
    
    std::filesystem::remove(snapshot_path);
    std::cout << "  PASSED" << std::endl;
}

void test_snapshot_crc_validation() {
    std::cout << "Test: Snapshot CRC64 Validation..." << std::endl;
    
    const std::string snapshot_path = "/tmp/test_crc.snap";
    std::filesystem::remove(snapshot_path);
    
    // Create snapshot
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        ht.insert("key1", ValueEntry("value1", 0));
        ht.insert("key2", ValueEntry("value2", 0));
        ht.create_snapshot(snapshot_path);
    }
    
    // Corrupt snapshot by modifying a byte
    {
        std::fstream file(snapshot_path, std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(50);  // Seek to middle of file
        char corrupt = 'X';
        file.write(&corrupt, 1);
        file.close();
    }
    
    // Try to load corrupted snapshot
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        bool success = ht.load_snapshot(snapshot_path);
        
        // Should fail CRC check
        assert(!success);
    }
    
    std::filesystem::remove(snapshot_path);
    std::cout << "  PASSED" << std::endl;
}

void test_snapshot_wal_truncation() {
    std::cout << "Test: WAL Truncation After Snapshot..." << std::endl;
    
    const std::string wal_path = "/tmp/test_trunc.log";
    const std::string snapshot_path = "/tmp/test_trunc.snap";
    std::filesystem::remove(wal_path);
    std::filesystem::remove(snapshot_path);
    
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU, wal_path);
        
        // Write some data (goes to WAL)
        ht.insert("a", ValueEntry("1", 0));
        ht.insert("b", ValueEntry("2", 0));
        ht.insert("c", ValueEntry("3", 0));
        ht.flush_wal();
        
        size_t wal_size_before = ht.wal_size();
        assert(wal_size_before > 0);
        
        // Create snapshot
        ht.create_snapshot(snapshot_path);
        
        // Truncate WAL
        ht.truncate_wal();
        
        // WAL should be empty or minimal now
        size_t wal_size_after = ht.wal_size();
        assert(wal_size_after == 0);
        
        // New writes should still work
        ht.insert("d", ValueEntry("4", 0));
        ht.flush_wal();
        assert(ht.wal_size() > 0);
    }
    
    std::filesystem::remove(wal_path);
    std::filesystem::remove(snapshot_path);
    std::cout << "  PASSED" << std::endl;
}

void test_snapshot_wal_recovery() {
    std::cout << "Test: Snapshot + WAL Recovery..." << std::endl;
    
    const std::string wal_path = "/tmp/test_snap_wal.log";
    const std::string snapshot_path = "/tmp/test_snap_wal.snap";
    std::filesystem::remove(wal_path);
    std::filesystem::remove(snapshot_path);
    
    // Phase 1: Create initial state and snapshot
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU, wal_path);
        
        ht.insert("old:1", ValueEntry("data1", 0));
        ht.insert("old:2", ValueEntry("data2", 0));
        ht.insert("old:3", ValueEntry("data3", 0));
        ht.flush_wal();
        
        // Create snapshot and truncate WAL
        ht.create_snapshot(snapshot_path);
        ht.truncate_wal();
        
        // Add new data AFTER snapshot
        ht.insert("new:1", ValueEntry("new_data1", 0));
        ht.insert("new:2", ValueEntry("new_data2", 0));
        ht.remove("old:2");  // Delete one from snapshot
        ht.flush_wal();
        
        // Verify complete state
        assert(ht.size() == 4);  // 3 old - 1 deleted + 2 new
    }
    
    // Phase 2: Simulate crash and recover
    {
        HashTable ht(16, 1024 * 1024, EvictionPolicy::LRU);
        
        // Load snapshot first
        bool loaded = ht.load_snapshot(snapshot_path);
        assert(loaded);
        assert(ht.size() == 3);  // Just snapshot data
        
        // Replay WAL on top
        size_t replayed = ht.replay_wal(wal_path);
        assert(replayed == 3);  // 2 SETs + 1 DELETE
        
        // Verify final state matches pre-crash
        assert(ht.size() == 4);
        
        auto val = ht.get("old:1");
        assert(val && (*val)->value == "data1");
        
        val = ht.get("old:2");
        assert(!val);  // Should be deleted
        
        val = ht.get("old:3");
        assert(val && (*val)->value == "data3");
        
        val = ht.get("new:1");
        assert(val && (*val)->value == "new_data1");
        
        val = ht.get("new:2");
        assert(val && (*val)->value == "new_data2");
    }
    
    std::filesystem::remove(wal_path);
    std::filesystem::remove(snapshot_path);
    std::cout << "  PASSED" << std::endl;
}

void test_large_snapshot() {
    std::cout << "Test: Large Snapshot (1000 entries)..." << std::endl;
    
    const std::string snapshot_path = "/tmp/test_large.snap";
    std::filesystem::remove(snapshot_path);
    
    const size_t NUM_ENTRIES = 1000;
    
    // Create large dataset
    {
        HashTable ht(128, 1024 * 1024 * 10, EvictionPolicy::LRU);
        
        for (size_t i = 0; i < NUM_ENTRIES; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i) + "_with_some_data";
            ht.insert(key, ValueEntry(value, 0));
        }
        
        assert(ht.size() == NUM_ENTRIES);
        
        // Create snapshot
        bool success = ht.create_snapshot(snapshot_path);
        assert(success);
    }
    
    // Load and verify
    {
        HashTable ht(128, 1024 * 1024 * 10, EvictionPolicy::LRU);
        
        bool success = ht.load_snapshot(snapshot_path);
        assert(success);
        assert(ht.size() == NUM_ENTRIES);
        
        // Spot check some entries
        for (size_t i = 0; i < NUM_ENTRIES; i += 100) {
            std::string key = "key_" + std::to_string(i);
            auto val = ht.get(key);
            assert(val);
            
            std::string expected = "value_" + std::to_string(i) + "_with_some_data";
            assert((*val)->value == expected);
        }
    }
    
    std::filesystem::remove(snapshot_path);
    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "====================" << std::endl;
    std::cout << "Snapshot Tests" << std::endl;
    std::cout << "====================" << std::endl;
    
    test_snapshot_create_load();
    test_snapshot_crc_validation();
    test_snapshot_wal_truncation();
    test_snapshot_wal_recovery();
    test_large_snapshot();
    
    std::cout << "\nAll snapshot tests passed! (5/5)" << std::endl;
    return 0;
}
