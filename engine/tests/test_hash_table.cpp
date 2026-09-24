#include "hash_table.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace kvstore;

// Simple test framework
void test_basic_operations() {
    HashTable ht;
    
    // Test insert and get
    ValueEntry v1("value1", 0);
    ht.insert("key1", v1);
    
    auto result = ht.get("key1");
    assert(result.has_value());
    assert((*result)->value == "value1");
    
    // Test non-existent key
    auto result2 = ht.get("nonexistent");
    assert(!result2.has_value());
    
    std::cout << "✓ Basic operations test passed\n";
}

void test_update() {
    HashTable ht;
    
    ValueEntry v1("value1", 0);
    ht.insert("key1", v1);
    
    ValueEntry v2("value2", 0);
    ht.insert("key1", v2);
    
    auto result = ht.get("key1");
    assert(result.has_value());
    assert((*result)->value == "value2");
    
    std::cout << "✓ Update test passed\n";
}

void test_delete() {
    HashTable ht;
    
    ValueEntry v1("value1", 0);
    ht.insert("key1", v1);
    
    bool removed = ht.remove("key1");
    assert(removed);
    
    auto result = ht.get("key1");
    assert(!result.has_value());
    
    // Test removing non-existent key
    bool removed2 = ht.remove("nonexistent");
    assert(!removed2);
    
    std::cout << "✓ Delete test passed\n";
}

void test_resize() {
    HashTable ht(4); // Small initial capacity
    
    // Insert enough entries to trigger resize (load factor > 0.75)
    for (int i = 0; i < 10; i++) {
        ValueEntry v("value" + std::to_string(i), 0);
        ht.insert("key" + std::to_string(i), v);
    }
    
    // Verify all keys are still accessible after resize
    for (int i = 0; i < 10; i++) {
        auto result = ht.get("key" + std::to_string(i));
        assert(result.has_value());
        assert((*result)->value == "value" + std::to_string(i));
    }
    
    auto stats = ht.get_stats();
    assert(stats.num_entries == 10);
    assert(stats.load_factor <= 0.75);
    
    std::cout << "✓ Resize test passed\n";
}

void test_access_metadata() {
    HashTable ht;
    
    ValueEntry v1("value1", 0);
    ht.insert("key1", v1);
    
    auto result1 = ht.get("key1");
    assert(result1.has_value());
    uint64_t count1 = (*result1)->access_count;
    uint64_t time1 = (*result1)->last_access_ns;
    
    // Access again
    auto result2 = ht.get("key1");
    assert(result2.has_value());
    uint64_t count2 = (*result2)->access_count;
    uint64_t time2 = (*result2)->last_access_ns;
    
    assert(count2 > count1);
    assert(time2 >= time1);
    
    std::cout << "✓ Access metadata test passed\n";
}

void test_stats() {
    HashTable ht(8);
    
    // Insert some entries
    for (int i = 0; i < 5; i++) {
        ValueEntry v("value" + std::to_string(i), 0);
        ht.insert("key" + std::to_string(i), v);
    }
    
    auto stats = ht.get_stats();
    assert(stats.num_entries == 5);
    assert(stats.num_buckets == 8);
    assert(stats.load_factor == 5.0 / 8.0);
    
    std::cout << "✓ Stats test passed\n";
}

int main() {
    std::cout << "Running HashTable tests...\n";
    
    test_basic_operations();
    test_update();
    test_delete();
    test_resize();
    test_access_metadata();
    test_stats();
    
    std::cout << "\nAll tests passed! ✓\n";
    return 0;
}
