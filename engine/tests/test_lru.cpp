#include "hash_table.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace kvstore;

void test_lru_eviction() {
    // Create table with small memory limit (1KB)
    HashTable ht(16, 1024);
    
    // Insert entries that exceed memory limit
    // Each entry ~100 bytes, insert 15 entries = ~1.5KB
    for (int i = 0; i < 15; i++) {
        std::string value(100, 'a' + (i % 26));
        ValueEntry v(value, 0);
        ht.insert("key" + std::to_string(i), v);
    }
    
    // The first inserted keys should be evicted (LRU)
    // Check that early keys are gone
    auto result0 = ht.get("key0");
    assert(!result0.has_value()); // Should be evicted
    
    // Later keys should still exist
    auto result14 = ht.get("key14");
    assert(result14.has_value()); // Should still exist
    
    std::cout << "✓ LRU eviction test passed\n";
}

void test_lru_access_order() {
    // Create table with small memory limit
    HashTable ht(16, 500);
    
    // Insert 3 entries
    ValueEntry v1(std::string(100, 'a'), 0);
    ValueEntry v2(std::string(100, 'b'), 0);
    ValueEntry v3(std::string(100, 'c'), 0);
    
    ht.insert("key1", v1);
    ht.insert("key2", v2);
    ht.insert("key3", v3);
    
    // Access key1 to make it recently used
    auto r1 = ht.get("key1");
    assert(r1.has_value());
    
    // Now insert key4, which should evict key2 (least recently used)
    // because key1 was just accessed and key3 is most recent insert
    ValueEntry v4(std::string(200, 'd'), 0);
    ht.insert("key4", v4);
    
    // key1 should still exist (recently accessed)
    auto result1 = ht.get("key1");
    assert(result1.has_value());
    
    // key4 should exist (just inserted)
    auto result4 = ht.get("key4");
    assert(result4.has_value());
    
    std::cout << "✓ LRU access order test passed\n";
}

int main() {
    std::cout << "Running LRU eviction tests...\n";
    
    test_lru_eviction();
    test_lru_access_order();
    
    std::cout << "\nAll LRU tests passed! ✓\n";
    return 0;
}
