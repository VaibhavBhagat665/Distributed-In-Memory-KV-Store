#include "hash_table.h"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <filesystem>

using namespace kvstore;

constexpr size_t NUM_OPERATIONS = 100000;  // 100K operations
constexpr size_t KEY_SPACE = 10000;        // 10K unique keys

std::string generate_key(size_t i) {
    return "bench_key_" + std::to_string(i);
}

std::string generate_value(size_t i) {
    return "bench_value_" + std::to_string(i) + "_data_padding_for_realistic_size";
}

double benchmark_no_wal() {
    HashTable ht(128, 1024 * 1024 * 100, EvictionPolicy::LRU);
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 99);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
        std::string key = generate_key(i % KEY_SPACE);
        
        if (dist(rng) < 80) {
            // 80% reads
            ht.get(key);
        } else {
            // 20% writes
            ht.insert(key, ValueEntry(generate_value(i), 0));
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double seconds = duration.count() / 1000000.0;
    return NUM_OPERATIONS / seconds;
}

double benchmark_with_wal() {
    const std::string wal_path = "/tmp/bench_wal.log";
    std::filesystem::remove(wal_path);
    
    HashTable ht(128, 1024 * 1024 * 100, EvictionPolicy::LRU, wal_path);
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 99);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
        std::string key = generate_key(i % KEY_SPACE);
        
        if (dist(rng) < 80) {
            // 80% reads (not affected by WAL)
            ht.get(key);
        } else {
            // 20% writes (go to WAL)
            ht.insert(key, ValueEntry(generate_value(i), 0));
        }
    }
    
    ht.flush_wal();  // Final flush
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double seconds = duration.count() / 1000000.0;
    
    std::filesystem::remove(wal_path);
    return NUM_OPERATIONS / seconds;
}

void benchmark_recovery_time(size_t num_keys) {
    const std::string wal_path = "/tmp/recovery_wal.log";
    const std::string snapshot_path = "/tmp/recovery_snap.snap";
    std::filesystem::remove(wal_path);
    std::filesystem::remove(snapshot_path);
    
    // Create dataset
    {
        HashTable ht(256, 1024 * 1024 * 100, EvictionPolicy::LRU, wal_path);
        
        for (size_t i = 0; i < num_keys; ++i) {
            std::string key = "recovery_key_" + std::to_string(i);
            std::string value = "recovery_value_" + std::to_string(i) + "_with_data";
            ht.insert(key, ValueEntry(value, 0));
        }
        
        ht.flush_wal();
        
        // Create snapshot halfway
        if (num_keys >= 100) {
            ht.create_snapshot(snapshot_path);
            ht.truncate_wal();
            
            // Add more data after snapshot
            for (size_t i = num_keys / 2; i < num_keys; ++i) {
                std::string key = "recovery_key_" + std::to_string(i);
                std::string value = "updated_value_" + std::to_string(i);
                ht.insert(key, ValueEntry(value, 0));
            }
            ht.flush_wal();
        }
    }
    
    // Measure recovery time
    auto start = std::chrono::high_resolution_clock::now();
    
    {
        HashTable ht(256, 1024 * 1024 * 100, EvictionPolicy::LRU);
        
        if (std::filesystem::exists(snapshot_path)) {
            ht.load_snapshot(snapshot_path);
        }
        ht.replay_wal(wal_path);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  " << std::setw(10) << num_keys << " keys | "
              << std::setw(8) << duration.count() << " ms" << std::endl;
    
    std::filesystem::remove(wal_path);
    std::filesystem::remove(snapshot_path);
}

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "Persistence Overhead Benchmark" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Configuration: 100K operations, 80% GET / 20% SET" << std::endl;
    std::cout << std::endl;
    
    // Throughput comparison
    std::cout << "Throughput Comparison:" << std::endl;
    std::cout << "----------------------" << std::endl;
    
    double no_wal_ops = benchmark_no_wal();
    double with_wal_ops = benchmark_with_wal();
    
    double overhead_pct = ((no_wal_ops - with_wal_ops) / no_wal_ops) * 100.0;
    
    std::cout << "No WAL:       " << std::fixed << std::setprecision(0) << no_wal_ops << " ops/sec" << std::endl;
    std::cout << "With WAL:     " << std::fixed << std::setprecision(0) << with_wal_ops << " ops/sec" << std::endl;
    std::cout << "Overhead:     " << std::fixed << std::setprecision(1) << overhead_pct << "%" << std::endl;
    std::cout << std::endl;
    
    // Recovery time scaling
    std::cout << "Recovery Time (Snapshot + WAL Replay):" << std::endl;
    std::cout << "--------------------------------------" << std::endl;
    
    benchmark_recovery_time(1000);      // 1K keys
    benchmark_recovery_time(10000);     // 10K keys
    benchmark_recovery_time(100000);    // 100K keys
    
    std::cout << std::endl;
    std::cout << "Note: WAL batch size = 100 ops, fsync per batch" << std::endl;
    std::cout << "      Snapshot created at 50% mark, remaining ops in WAL" << std::endl;
    
    return 0;
}
