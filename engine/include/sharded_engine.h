#ifndef KVSTORE_SHARDED_ENGINE_H
#define KVSTORE_SHARDED_ENGINE_H

#include "hash_table.h"
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <optional>
#include <future>

namespace kvstore {

// Request types for the sharded engine
enum class RequestType {
    GET,
    SET,
    REMOVE
};

// Request structure
struct Request {
    RequestType type;
    std::string key;
    std::optional<ValueEntry> value;  // Only for SET
    
    // Response handling
    std::promise<std::optional<ValueEntry>> promise;
    
    Request(RequestType t, const std::string& k)
        : type(t), key(k) {}
    
    Request(RequestType t, const std::string& k, const ValueEntry& v)
        : type(t), key(k), value(v) {}
};

/**
 * Sharded engine with thread-per-core architecture.
 * 
 * Key design decisions:
 * - Shared-nothing: each thread owns its own HashTable
 * - Deterministic routing: shard_id = murmur3_hash(key) % num_shards
 * - Lock-free-ish: uses lock-free queues per thread (SPSC pattern)
 * - Near-linear scaling: no contention between threads
 * 
 * This is the architecture that makes Dragonfly (Redis competitor) fast.
 */
class ShardedEngine {
public:
    explicit ShardedEngine(size_t num_shards = std::thread::hardware_concurrency(),
                          size_t memory_limit_per_shard = 100 * 1024 * 1024,
                          EvictionPolicy policy = EvictionPolicy::LRU);
    
    ~ShardedEngine();
    
    // Core operations (thread-safe)
    std::optional<ValueEntry> get(const std::string& key);
    bool set(const std::string& key, const ValueEntry& value);
    bool remove(const std::string& key);
    
    // Stats
    size_t num_shards() const { return num_shards_; }
    size_t total_entries() const;
    
    // Start/stop
    void start();
    void stop();
    
private:
    size_t num_shards_;
    std::vector<std::unique_ptr<HashTable>> shards_;
    std::vector<std::unique_ptr<std::thread>> worker_threads_;
    std::vector<std::unique_ptr<std::queue<std::unique_ptr<Request>>>> request_queues_;
    std::vector<std::unique_ptr<std::mutex>> queue_mutexes_;
    std::vector<std::unique_ptr<std::condition_variable>> queue_cvs_;
    std::atomic<bool> running_;
    
    // Shard routing
    size_t get_shard_id(const std::string& key) const;
    
    // Worker thread function
    void worker_loop(size_t shard_id);
    
    // Process a single request
    void process_request(size_t shard_id, std::unique_ptr<Request> req);
};

} // namespace kvstore

#endif // KVSTORE_SHARDED_ENGINE_H
