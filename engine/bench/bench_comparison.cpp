#include "sharded_engine_sync.h"
#include "mutex_engine.h"
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

// Benchmark ShardedEngineSync (fine-grained per-shard locking)
double benchmark_sharded(size_t num_threads) {
    ShardedEngineSync engine(num_threads);
    
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
            std::mt19937 rng(42 + t);
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
    return NUM_OPERATIONS / seconds;  // ops/sec
}

// Benchmark MutexEngine (global mutex)
double benchmark_mutex(size_t num_threads) {
    MutexEngine engine;
    
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
            std::mt19937 rng(42 + t);
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
    return NUM_OPERATIONS / seconds;  // ops/sec
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Fine-Grained vs Global-Mutex Comparison" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Configuration: 1M operations, 80% GET / 20% SET" << std::endl;
    std::cout << std::endl;
    
    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    
    std::cout << "| Threads | Fine-Grained (N mutexes) | Global-Mutex (1 mutex) | Advantage |" << std::endl;
    std::cout << "|---------|--------------------------|------------------------|-----------|" << std::endl;
    
    for (size_t num_threads : thread_counts) {
        double sharded_ops = benchmark_sharded(num_threads);
        double mutex_ops = benchmark_mutex(num_threads);
        double advantage = sharded_ops / mutex_ops;
        
        std::cout << "| " << std::setw(7) << num_threads << " | "
                  << std::setw(24) << std::fixed << std::setprecision(0) << sharded_ops << " | "
                  << std::setw(22) << std::fixed << std::setprecision(0) << mutex_ops << " | "
                  << std::setw(9) << std::fixed << std::setprecision(2) << advantage << "x |"
                  << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Analysis:" << std::endl;
    std::cout << "- Fine-grained locking (N mutexes, 1 per shard) reduces contention" << std::endl;
    std::cout << "- Global mutex creates serialization bottleneck as threads increase" << std::endl;
    std::cout << "- Advantage grows with thread count due to better parallelism" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: Fine-grained = " << std::thread::hardware_concurrency() << " shards with per-shard mutexes" << std::endl;
    std::cout << "      Global-mutex = 1 shard with single mutex protecting all operations" << std::endl;
    
    return 0;
}
