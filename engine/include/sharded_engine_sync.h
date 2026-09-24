#ifndef KVSTORE_SHARDED_ENGINE_SYNC_H
#define KVSTORE_SHARDED_ENGINE_SYNC_H

#include "hash_table.h"
#include <vector>
#include <mutex>
#include <optional>
#include <thread>

namespace kvstore {

/**
 * Synchronous sharded engine for fair benchmark comparison.
 * 
 * Key differences from ShardedEngine:
 * - Direct function calls (no promises/futures/queues)
 * - Per-shard mutexes (fine-grained locking)
 * - Same deterministic routing as ShardedEngine
 * 
 * This allows fair comparison with MutexEngine (single mutex).
 */
class ShardedEngineSync {
public:
    explicit ShardedEngineSync(size_t num_shards = std::thread::hardware_concurrency(),
                               size_t memory_limit_per_shard = 100 * 1024 * 1024,
                               EvictionPolicy policy = EvictionPolicy::LRU)
        : num_shards_(num_shards) {
        
        for (size_t i = 0; i < num_shards_; ++i) {
            shards_.emplace_back(std::make_unique<HashTable>(16, memory_limit_per_shard, policy));
            shard_mutexes_.emplace_back(std::make_unique<std::mutex>());
        }
    }
    
    std::optional<ValueEntry> get(const std::string& key) {
        size_t shard_id = get_shard_id(key);
        std::lock_guard<std::mutex> lock(*shard_mutexes_[shard_id]);
        
        auto result = shards_[shard_id]->get(key);
        if (result.has_value()) {
            return **result;
        }
        return std::nullopt;
    }
    
    bool set(const std::string& key, const ValueEntry& value) {
        size_t shard_id = get_shard_id(key);
        std::lock_guard<std::mutex> lock(*shard_mutexes_[shard_id]);
        return shards_[shard_id]->insert(key, value);
    }
    
    bool remove(const std::string& key) {
        size_t shard_id = get_shard_id(key);
        std::lock_guard<std::mutex> lock(*shard_mutexes_[shard_id]);
        return shards_[shard_id]->remove(key);
    }
    
    size_t num_shards() const { return num_shards_; }
    
    size_t total_entries() const {
        size_t total = 0;
        for (size_t i = 0; i < num_shards_; ++i) {
            std::lock_guard<std::mutex> lock(*shard_mutexes_[i]);
            total += shards_[i]->size();
        }
        return total;
    }

private:
    size_t num_shards_;
    std::vector<std::unique_ptr<HashTable>> shards_;
    std::vector<std::unique_ptr<std::mutex>> shard_mutexes_;
    
    // MurmurHash3-inspired hash function
    uint32_t murmur3_hash(const std::string& key) const {
        const uint32_t c1 = 0xcc9e2d51;
        const uint32_t c2 = 0x1b873593;
        const uint32_t r1 = 15;
        const uint32_t r2 = 13;
        const uint32_t m = 5;
        const uint32_t n = 0xe6546b64;
        
        uint32_t hash = 0;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(key.data());
        size_t len = key.length();
        
        for (size_t i = 0; i < len; ++i) {
            uint32_t k = data[i];
            k *= c1;
            k = (k << r1) | (k >> (32 - r1));
            k *= c2;
            
            hash ^= k;
            hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
        }
        
        hash ^= len;
        hash ^= (hash >> 16);
        hash *= 0x85ebca6b;
        hash ^= (hash >> 13);
        hash *= 0xc2b2ae35;
        hash ^= (hash >> 16);
        
        return hash;
    }
    
    size_t get_shard_id(const std::string& key) const {
        return murmur3_hash(key) % num_shards_;
    }
};

} // namespace kvstore

#endif // KVSTORE_SHARDED_ENGINE_SYNC_H
