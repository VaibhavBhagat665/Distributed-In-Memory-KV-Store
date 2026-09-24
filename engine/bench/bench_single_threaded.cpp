#include "hash_table.h"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>

using namespace kvstore;

// Benchmark configuration
constexpr size_t NUM_OPERATIONS = 1000000; // 1M operations
constexpr size_t KEY_SPACE = 100000;       // 100K unique keys

std::string generate_key(size_t i) {
    return "key_" + std::to_string(i);
}

std::string generate_value(size_t i) {
    return "value_" + std::to_string(i) + "_data";
}

void benchmark_set(HashTable& ht) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
        std::string key = generate_key(i % KEY_SPACE);
        ValueEntry entry(generate_value(i));
        ht.insert(key, entry);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double seconds = duration.count() / 1000000.0;
    double ops_per_sec = NUM_OPERATIONS / seconds;
    
    std::cout << "SET Benchmark (single-threaded):" << std::endl;
    std::cout << "  Operations: " << NUM_OPERATIONS << std::endl;
    std::cout << "  Duration: " << std::fixed << std::setprecision(2) << seconds << " seconds" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec" << std::endl;
    std::cout << std::endl;
}

void benchmark_get(HashTable& ht) {
    // Pre-populate
    for (size_t i = 0; i < KEY_SPACE; ++i) {
        std::string key = generate_key(i);
        ValueEntry entry(generate_value(i));
        ht.insert(key, entry);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
        std::string key = generate_key(i % KEY_SPACE);
        auto result = ht.get(key);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double seconds = duration.count() / 1000000.0;
    double ops_per_sec = NUM_OPERATIONS / seconds;
    
    std::cout << "GET Benchmark (single-threaded):" << std::endl;
    std::cout << "  Operations: " << NUM_OPERATIONS << std::endl;
    std::cout << "  Duration: " << std::fixed << std::setprecision(2) << seconds << " seconds" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec" << std::endl;
    std::cout << std::endl;
}

void benchmark_mixed(HashTable& ht) {
    // Pre-populate
    for (size_t i = 0; i < KEY_SPACE; ++i) {
        std::string key = generate_key(i);
        ValueEntry entry(generate_value(i));
        ht.insert(key, entry);
    }
    
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(0, 99);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
        std::string key = generate_key(i % KEY_SPACE);
        
        // 80% GET, 20% SET (typical workload)
        if (dist(rng) < 80) {
            auto result = ht.get(key);
        } else {
            ValueEntry entry(generate_value(i));
            ht.insert(key, entry);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double seconds = duration.count() / 1000000.0;
    double ops_per_sec = NUM_OPERATIONS / seconds;
    
    std::cout << "MIXED Benchmark (80% GET, 20% SET, single-threaded):" << std::endl;
    std::cout << "  Operations: " << NUM_OPERATIONS << std::endl;
    std::cout << "  Duration: " << std::fixed << std::setprecision(2) << seconds << " seconds" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Single-Threaded Storage Engine Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Run benchmarks with default settings
    {
        HashTable ht;
        benchmark_set(ht);
    }
    
    {
        HashTable ht;
        benchmark_get(ht);
    }
    
    {
        HashTable ht;
        benchmark_mixed(ht);
    }
    
    std::cout << "Benchmark complete!" << std::endl;
    return 0;
}
