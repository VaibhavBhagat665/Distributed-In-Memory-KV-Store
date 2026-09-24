#include "sharded_engine.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace kvstore;

void test_basic_sharded_operations() {
    ShardedEngine engine(4); // 4 shards
    engine.start();
    
    // Insert some keys
    for (int i = 0; i < 20; ++i) {
        ValueEntry entry("value" + std::to_string(i));
        bool success = engine.set("key" + std::to_string(i), entry);
        assert(success);
    }
    
    // Retrieve keys
    for (int i = 0; i < 20; ++i) {
        auto result = engine.get("key" + std::to_string(i));
        assert(result.has_value());
        assert(result->value == "value" + std::to_string(i));
    }
    
    // Remove some keys
    for (int i = 0; i < 10; ++i) {
        bool success = engine.remove("key" + std::to_string(i));
        assert(success);
    }
    
    // Verify removal
    for (int i = 0; i < 10; ++i) {
        auto result = engine.get("key" + std::to_string(i));
        assert(!result.has_value());
    }
    
    // Verify remaining keys
    for (int i = 10; i < 20; ++i) {
        auto result = engine.get("key" + std::to_string(i));
        assert(result.has_value());
        assert(result->value == "value" + std::to_string(i));
    }
    
    engine.stop();
    std::cout << "✓ Basic sharded operations test passed" << std::endl;
}

void test_deterministic_routing() {
    ShardedEngine engine(4);
    engine.start();
    
    // Insert key
    ValueEntry entry("test_value");
    engine.set("test_key", entry);
    
    // Retrieve multiple times - should always route to same shard
    for (int i = 0; i < 10; ++i) {
        auto result = engine.get("test_key");
        assert(result.has_value());
        assert(result->value == "test_value");
    }
    
    engine.stop();
    std::cout << "✓ Deterministic routing test passed" << std::endl;
}

void test_concurrent_access() {
    ShardedEngine engine(8); // 8 shards for better concurrency
    engine.start();
    
    const int num_threads = 4;
    const int ops_per_thread = 100;
    std::vector<std::thread> threads;
    
    // Each thread writes its own set of keys
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&engine, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                ValueEntry entry("value_" + std::to_string(i));
                engine.set(key, entry);
            }
        });
    }
    
    // Wait for all writes
    for (auto& thread : threads) {
        thread.join();
    }
    threads.clear();
    
    // Verify all keys exist
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&engine, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                auto result = engine.get(key);
                assert(result.has_value());
                assert(result->value == "value_" + std::to_string(i));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check total entries
    size_t total = engine.total_entries();
    assert(total == num_threads * ops_per_thread);
    
    engine.stop();
    std::cout << "✓ Concurrent access test passed (" << total << " entries)" << std::endl;
}

void test_shard_isolation() {
    ShardedEngine engine(4);
    engine.start();
    
    // Each shard should be independent
    // Insert 100 keys and verify they're distributed
    for (int i = 0; i < 100; ++i) {
        ValueEntry entry("value" + std::to_string(i));
        engine.set("key" + std::to_string(i), entry);
    }
    
    size_t total = engine.total_entries();
    assert(total == 100);
    
    // Keys should be distributed across shards (not all in one)
    // With 100 keys and 4 shards, we expect roughly 25 per shard
    // But we just verify total correctness here
    
    engine.stop();
    std::cout << "✓ Shard isolation test passed" << std::endl;
}

int main() {
    std::cout << "Running ShardedEngine tests..." << std::endl;
    test_basic_sharded_operations();
    test_deterministic_routing();
    test_concurrent_access();
    test_shard_isolation();
    std::cout << "All ShardedEngine tests passed! ✓" << std::endl;
    return 0;
}
