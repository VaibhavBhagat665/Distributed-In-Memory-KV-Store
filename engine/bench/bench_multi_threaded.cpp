#include "sharded_engine.h"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <thread>
#include <vector>

using namespace kvstore;

// Benchmark configuration
constexpr size_t NUM_OPERATIONS = 1000000; // 1M operations total
constexpr size_t KEY_SPACE = 100000;       // 100K unique keys

std::string generate_key(size_t i) {
    return "key_" + std::to_string(i);
}

std::string generate_value(size_t i) {
    return "value_" + std::to_string(i) + "_data";
}

void benchmark_sharded_mixed(size_t num_threads) {
    ShardedEngine engine(num_threads);
    engine.start();
    
    // Pre-populate
    for (size_t i = 0; i < KEY_SPACE; ++i) {
        std::string key = generate_key(i);
        ValueEntry entry(generate_value(i));
        engine.set(key, entry);
    }
    
    const size_t ops_per_thread = NUM_OPERATIONS / num_threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Launch worker threads
    std::vector<std::thread> threads;
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&engine, t, ops_per_thread]() {
            std::mt19937 rng(42 + t); // Different seed per thread
            std::uniform_int_distribution<int> dist(0, 99);
            
            for (size_t i = 0; i < ops_per_thread; ++i) {
                std::string key = generate_key((t * ops_per_thread + i) % KEY_SPACE);
                
                // 80% GET, 20% SET
                if (dist(rng) < 80) {
                    auto result = engine.get(key);
                } else {
                    ValueEntry entry(generate_value(t * ops_per_thread + i));
                    engine.set(key, entry);
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double seconds = duration.count() / 1000000.0;
    double ops_per_sec = NUM_OPERATIONS / seconds;
    
    std::cout << "Threads: " << num_threads 
              << " | Duration: " << std::fixed << std::setprecision(2) << seconds << "s"
              << " | Throughput: " << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec" << std::endl;
    
    engine.stop();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Multi-Threaded Storage Engine Benchmark" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Configuration: 1M operations, 80% GET / 20% SET" << std::endl;
    std::cout << std::endl;
    
    // Benchmark with different thread counts
    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    
    std::cout << "Scaling Results:" << std::endl;
    for (size_t num_threads : thread_counts) {
        benchmark_sharded_mixed(num_threads);
    }
    
    std::cout << std::endl;
    std::cout << "Benchmark complete!" << std::endl;
    std::cout << "Note: Near-linear scaling indicates good shared-nothing design." << std::endl;
    return 0;
}
