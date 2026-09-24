#include "hash_table.h"
#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>

using namespace kvstore;

uint64_t current_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void test_ttl_lazy_expiration() {
    HashTable ht;
    
    uint64_t now = current_time_ns();
    uint64_t ttl_100ms = 100 * 1000 * 1000; // 100ms in nanoseconds
    
    // Insert key with 100ms TTL
    ValueEntry entry1("value1");
    entry1.expiry_ns = now + ttl_100ms;
    ht.insert("key1", entry1);
    
    // Insert key with no TTL
    ValueEntry entry2("value2");
    ht.insert("key2", entry2);
    
    // Immediately check - should exist
    auto result1 = ht.get("key1");
    assert(result1.has_value());
    assert((*result1)->value == "value1");
    
    auto result2 = ht.get("key2");
    assert(result2.has_value());
    assert((*result2)->value == "value2");
    
    // Wait for expiration (150ms to be safe)
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // key1 should be expired (lazy check on GET)
    auto result1_expired = ht.get("key1");
    assert(!result1_expired.has_value()); // Should return null
    
    // key2 should still exist (no TTL)
    auto result2_still = ht.get("key2");
    assert(result2_still.has_value());
    assert((*result2_still)->value == "value2");
    
    std::cout << "✓ TTL lazy expiration test passed" << std::endl;
}

void test_ttl_active_expiration() {
    HashTable ht;
    
    // Start TTL scanner with 50ms interval
    ht.start_ttl_scanner(50);
    
    uint64_t now = current_time_ns();
    uint64_t ttl_100ms = 100 * 1000 * 1000; // 100ms in nanoseconds
    
    // Insert multiple keys with short TTL
    for (int i = 0; i < 10; ++i) {
        ValueEntry entry("value" + std::to_string(i));
        entry.expiry_ns = now + ttl_100ms;
        ht.insert("key" + std::to_string(i), entry);
    }
    
    // Verify all keys exist initially
    size_t initial_size = ht.size();
    assert(initial_size == 10);
    
    // Wait for TTL to expire + scanner to run (200ms to be safe)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Keys should be actively removed by background scanner
    // Size should be reduced (some or all keys removed)
    size_t final_size = ht.size();
    assert(final_size < initial_size);
    
    // Stop scanner
    ht.stop_ttl_scanner();
    
    std::cout << "✓ TTL active expiration test passed (initial: " 
              << initial_size << ", final: " << final_size << ")" << std::endl;
}

void test_ttl_no_expiry() {
    HashTable ht;
    
    // Insert keys with expiry_ns = 0 (no expiry)
    ValueEntry entry1("value1");
    entry1.expiry_ns = 0;
    ht.insert("key1", entry1);
    
    ValueEntry entry2("value2");
    // Default is 0
    ht.insert("key2", entry2);
    
    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Both should still exist
    auto result1 = ht.get("key1");
    assert(result1.has_value());
    
    auto result2 = ht.get("key2");
    assert(result2.has_value());
    
    std::cout << "✓ TTL no expiry test passed" << std::endl;
}

void test_ttl_update_resets() {
    HashTable ht;
    
    uint64_t now = current_time_ns();
    uint64_t ttl_100ms = 100 * 1000 * 1000;
    
    // Insert key with 100ms TTL
    ValueEntry entry1("value1");
    entry1.expiry_ns = now + ttl_100ms;
    ht.insert("key1", entry1);
    
    // Wait 60ms
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    // Update with new value and new TTL
    now = current_time_ns();
    ValueEntry entry2("value2");
    entry2.expiry_ns = now + ttl_100ms;
    ht.insert("key1", entry2);
    
    // Wait another 60ms (120ms total from first insert, but only 60ms from update)
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    // Key should still exist (updated TTL)
    auto result = ht.get("key1");
    assert(result.has_value());
    assert((*result)->value == "value2");
    
    // Wait another 60ms (now 120ms from update)
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    // Now should be expired
    auto result_expired = ht.get("key1");
    assert(!result_expired.has_value());
    
    std::cout << "✓ TTL update resets test passed" << std::endl;
}

int main() {
    std::cout << "Running TTL expiration tests..." << std::endl;
    test_ttl_lazy_expiration();
    test_ttl_active_expiration();
    test_ttl_no_expiry();
    test_ttl_update_resets();
    std::cout << "All TTL tests passed! ✓" << std::endl;
    return 0;
}
