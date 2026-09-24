#include "hash_table.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace kvstore;

void test_lfu_eviction() {
    // Create hash table with 150 bytes memory limit, using LFU
    HashTable ht(16, 150, EvictionPolicy::LFU);
    
    // Insert 15 keys, each value is 10 bytes = 150 bytes total
    for (int i = 0; i < 15; ++i) {
        ValueEntry entry("value_val_" + std::to_string(i)); // 10 bytes each
        ht.insert("key" + std::to_string(i), entry);
    }
    
    // Access keys with different frequencies
    // key0: access 5 times (access_count = 5)
    for (int i = 0; i < 5; ++i) {
        auto result0 = ht.get("key0");
        assert(result0.has_value());
    }
    
    // key14: access 1 time (access_count = 1)
    auto result14 = ht.get("key14");
    assert(result14.has_value());
    
    // key1-13: not accessed (access_count = 0)
    
    // Insert one more key (16th key) - should trigger eviction
    // Should evict one of the keys with access_count = 0 (key1-13)
    ValueEntry entry16("value_val16");
    ht.insert("key16", entry16);
    
    // key0 (most frequently accessed) should still exist
    auto check0 = ht.get("key0");
    assert(check0.has_value());
    
    // key14 (accessed once) should still exist
    auto check14 = ht.get("key14");
    assert(check14.has_value());
    
    // key16 (just inserted) should exist
    auto check16 = ht.get("key16");
    assert(check16.has_value());
    
    // At least one of the unaccessed keys (key1-13) should be evicted
    int evicted_count = 0;
    for (int i = 1; i < 14; ++i) {
        auto check = ht.get("key" + std::to_string(i));
        if (!check.has_value()) {
            evicted_count++;
        }
    }
    assert(evicted_count >= 1);
    
    std::cout << "✓ LFU eviction test passed" << std::endl;
}

void test_lfu_frequency_order() {
    // Create hash table with 100 bytes memory limit, using LFU
    HashTable ht(16, 100, EvictionPolicy::LFU);
    
    // Insert 10 keys (10 bytes each = 100 bytes)
    for (int i = 0; i < 10; ++i) {
        ValueEntry entry("value_val" + std::to_string(i)); // 10 bytes
        ht.insert("key" + std::to_string(i), entry);
    }
    
    // Access with different frequencies:
    // key1: 10 times
    for (int i = 0; i < 10; ++i) {
        auto r1 = ht.get("key1");
        assert(r1.has_value());
    }
    
    // key2: 5 times
    for (int i = 0; i < 5; ++i) {
        auto r2 = ht.get("key2");
        assert(r2.has_value());
    }
    
    // key3: 2 times
    for (int i = 0; i < 2; ++i) {
        auto r3 = ht.get("key3");
        assert(r3.has_value());
    }
    
    // key0, key4-9: not accessed (frequency = 0)
    
    // Insert 5 more keys to trigger eviction
    for (int i = 10; i < 15; ++i) {
        ValueEntry entry("value_val" + std::to_string(i));
        ht.insert("key" + std::to_string(i), entry);
    }
    
    // Most frequently accessed keys should still exist
    auto result1 = ht.get("key1");  // 10 accesses
    assert(result1.has_value());
    
    auto result2 = ht.get("key2");  // 5 accesses
    assert(result2.has_value());
    
    auto result3 = ht.get("key3");  // 2 accesses
    assert(result3.has_value());
    
    // Some of the never-accessed keys should be evicted
    int unaccessed_evicted = 0;
    for (int i = 4; i < 10; ++i) {
        auto check = ht.get("key" + std::to_string(i));
        if (!check.has_value()) {
            unaccessed_evicted++;
        }
    }
    
    // At least some unaccessed keys should be evicted
    assert(unaccessed_evicted > 0);
    
    std::cout << "✓ LFU frequency order test passed" << std::endl;
}

int main() {
    std::cout << "Running LFU eviction tests..." << std::endl;
    test_lfu_eviction();
    test_lfu_frequency_order();
    std::cout << "All LFU tests passed! ✓" << std::endl;
    return 0;
}
