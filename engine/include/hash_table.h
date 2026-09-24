#ifndef KVSTORE_HASH_TABLE_H
#define KVSTORE_HASH_TABLE_H

#include "lru_policy.h"
#include "lfu_policy.h"
#include "wal.h"
#include <string>
#include <vector>
#include <list>
#include <cstdint>
#include <functional>
#include <optional>
#include <memory>
#include <thread>
#include <atomic>

namespace kvstore {

// Forward declarations
class SnapshotWriter;
class SnapshotReader;

// Eviction policy type
enum class EvictionPolicy {
    LRU,  // Least Recently Used
    LFU   // Least Frequently Used
};

// Value entry storing the actual data and metadata
struct ValueEntry {
    std::string value;
    uint64_t access_count;    // For LFU eviction
    uint64_t last_access_ns;  // For LRU eviction
    uint64_t expiry_ns;       // For TTL (0 = no expiry)
    
    ValueEntry(const std::string& val, uint64_t ttl_ns = 0)
        : value(val), access_count(0), last_access_ns(0), expiry_ns(ttl_ns) {}
};

// Key-value pair with cached hash for efficient operations
struct KeyValuePair {
    std::string key;
    ValueEntry value;
    uint64_t hash;  // Cached hash to avoid recomputation
    
    KeyValuePair(const std::string& k, const ValueEntry& v, uint64_t h)
        : key(k), value(v), hash(h) {}
};

// Hash table statistics for monitoring
struct HashTableStats {
    size_t num_entries;
    size_t num_buckets;
    double load_factor;
    size_t num_collisions;
    size_t max_chain_length;
};

/**
 * Custom hash table with chaining for collision resolution.
 * 
 * Design decisions:
 * - Chaining chosen over open addressing for:
 *   1. Simplicity of implementation
 *   2. Iterator stability (no rehashing during iteration)
 *   3. Predictable performance under high load
 * 
 * - Load factor threshold: 0.75
 *   When exceeded, table resizes to maintain O(1) average-case operations
 * 
 * - NOT thread-safe: Each thread owns its own hash table (shared-nothing architecture)
 */
class HashTable {
public:
    explicit HashTable(size_t initial_capacity = 16, 
                      size_t memory_limit = 1024 * 1024 * 100,  // 100MB default
                      EvictionPolicy policy = EvictionPolicy::LRU,
                      const std::string& wal_path = "");  // Empty = no WAL
    
    // Core operations
    bool insert(const std::string& key, const ValueEntry& value);
    std::optional<ValueEntry*> get(const std::string& key);
    bool remove(const std::string& key);
    
    // Utility
    size_t size() const { return num_entries_; }
    bool empty() const { return num_entries_ == 0; }
    HashTableStats get_stats() const;
    
    // WAL management
    void enable_wal(const std::string& wal_path, size_t batch_size = 100);
    void flush_wal();
    size_t wal_size() const;
    
    // Snapshot management
    bool create_snapshot(const std::string& snapshot_path);
    bool load_snapshot(const std::string& snapshot_path);
    void truncate_wal();  // Truncate WAL after successful snapshot
    
    // Recovery
    size_t replay_wal(const std::string& wal_path);
    
    // TTL management
    void start_ttl_scanner(uint64_t scan_interval_ms = 100);
    void stop_ttl_scanner();
    ~HashTable();
    
    // For testing: iterate over all entries
    template<typename Func>
    void for_each(Func&& func) {
        for (auto& chain : buckets_) {
            for (auto& kv : chain) {
                func(kv.key, kv.value);
            }
        }
    }
    
private:
    using Chain = std::list<KeyValuePair>;
    std::vector<Chain> buckets_;
    size_t num_entries_;
    EvictionPolicy eviction_policy_;
    std::unique_ptr<LRUPolicy> lru_policy_;
    std::unique_ptr<LFUPolicy> lfu_policy_;
    std::unique_ptr<WALWriter> wal_writer_;
    std::string wal_path_;  // Store path for truncation
    static constexpr double LOAD_FACTOR_THRESHOLD = 0.75;
    
    // TTL scanner thread
    std::unique_ptr<std::thread> ttl_scanner_thread_;
    std::atomic<bool> stop_scanner_;
    uint64_t scan_interval_ms_;
    
    // Hash function: MurmurHash3-inspired (simplified)
    uint64_t hash(const std::string& key) const;
    
    // Resize and rehash when load factor exceeds threshold
    void resize_if_needed();
    void rehash(size_t new_capacity);
    
    // Eviction
    void evict_if_needed();
    
    // TTL expiration
    void scan_expired_keys();
    bool is_expired(const ValueEntry& entry) const;
};

} // namespace kvstore

#endif // KVSTORE_HASH_TABLE_H
