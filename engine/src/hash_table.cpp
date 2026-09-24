#include "hash_table.h"
#include "snapshot.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

namespace kvstore {

HashTable::HashTable(size_t initial_capacity, size_t memory_limit, EvictionPolicy policy, const std::string& wal_path)
    : buckets_(initial_capacity), num_entries_(0), eviction_policy_(policy),
      stop_scanner_(false), scan_interval_ms_(100) {
    // Ensure capacity is power of 2 for efficient modulo via bitwise AND
    size_t capacity = 1;
    while (capacity < initial_capacity) {
        capacity <<= 1;
    }
    buckets_.resize(capacity);
    
    // Initialize the appropriate eviction policy
    if (eviction_policy_ == EvictionPolicy::LRU) {
        lru_policy_ = std::make_unique<LRUPolicy>(memory_limit);
    } else {
        lfu_policy_ = std::make_unique<LFUPolicy>(memory_limit);
    }
    
    // Initialize WAL if path provided
    if (!wal_path.empty()) {
        enable_wal(wal_path);
    }
}

uint64_t HashTable::hash(const std::string& key) const {
    // Simplified MurmurHash3-inspired hash function
    // Good avalanche properties for string keys
    uint64_t h = 0x9e3779b97f4a7c15ULL; // Golden ratio constant
    
    for (unsigned char c : key) {
        h ^= c;
        h *= 0x100000001b3ULL; // FNV prime
    }
    
    // Final mixing
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    
    return h;
}

bool HashTable::insert(const std::string& key, const ValueEntry& value) {
    // Write to WAL before applying change
    if (wal_writer_) {
        WALRecord record(WALOpType::SET, key, value.value, value.expiry_ns);
        wal_writer_->append(record);
    }
    
    // Try to evict if needed before insert
    evict_if_needed();
    
    resize_if_needed();
    
    uint64_t h = hash(key);
    size_t bucket_idx = h % buckets_.size();
    Chain& chain = buckets_[bucket_idx];
    
    // Check if key already exists (update case)
    for (auto& kv : chain) {
        if (kv.key == key) {
            size_t old_size = kv.value.value.size();
            kv.value = value;
            kv.hash = h;
            
            // Update eviction policy tracking
            if (eviction_policy_ == EvictionPolicy::LRU) {
                lru_policy_->on_delete(key, old_size);
                lru_policy_->on_insert(key, value.value.size(), value.last_access_ns);
            } else {
                lfu_policy_->on_delete(key, old_size);
                lfu_policy_->on_insert(key, value.value.size(), value.access_count);
            }
            return true;
        }
    }
    
    // Insert new entry
    chain.emplace_back(key, value, h);
    ++num_entries_;
    
    // Track in eviction policy
    if (eviction_policy_ == EvictionPolicy::LRU) {
        lru_policy_->on_insert(key, value.value.size(), value.last_access_ns);
    } else {
        lfu_policy_->on_insert(key, value.value.size(), value.access_count);
    }
    
    return true;
}

std::optional<ValueEntry*> HashTable::get(const std::string& key) {
    uint64_t h = hash(key);
    size_t bucket_idx = h % buckets_.size();
    Chain& chain = buckets_[bucket_idx];
    
    for (auto& kv : chain) {
        if (kv.key == key) {
            // Lazy TTL check: if expired, return null and mark for deletion
            if (is_expired(kv.value)) {
                // Will be cleaned up by active scanner or next operation
                return std::nullopt;
            }
            
            // Update access metadata
            kv.value.access_count++;
            kv.value.last_access_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            
            // Update eviction policy tracking
            if (eviction_policy_ == EvictionPolicy::LRU) {
                lru_policy_->on_access(key, kv.value.last_access_ns);
            } else {
                lfu_policy_->on_access(key, kv.value.access_count);
            }
            
            return &kv.value;
        }
    }
    
    return std::nullopt;
}

bool HashTable::remove(const std::string& key) {
    // Write to WAL before applying change
    if (wal_writer_) {
        WALRecord record(WALOpType::DELETE, key);
        wal_writer_->append(record);
    }
    
    uint64_t h = hash(key);
    size_t bucket_idx = h % buckets_.size();
    Chain& chain = buckets_[bucket_idx];
    
    for (auto it = chain.begin(); it != chain.end(); ++it) {
        if (it->key == key) {
            size_t value_size = it->value.value.size();
            chain.erase(it);
            --num_entries_;
            
            // Update eviction policy tracking
            if (eviction_policy_ == EvictionPolicy::LRU) {
                lru_policy_->on_delete(key, value_size);
            } else {
                lfu_policy_->on_delete(key, value_size);
            }
            
            return true;
        }
    }
    
    return false;
}

void HashTable::resize_if_needed() {
    double load_factor = static_cast<double>(num_entries_) / buckets_.size();
    if (load_factor > LOAD_FACTOR_THRESHOLD) {
        rehash(buckets_.size() * 2);
    }
}

void HashTable::rehash(size_t new_capacity) {
    std::vector<Chain> new_buckets(new_capacity);
    
    // Rehash all existing entries
    for (auto& chain : buckets_) {
        for (auto& kv : chain) {
            size_t new_bucket_idx = kv.hash % new_capacity;
            new_buckets[new_bucket_idx].push_back(std::move(kv));
        }
    }
    
    buckets_ = std::move(new_buckets);
}

HashTableStats HashTable::get_stats() const {
    HashTableStats stats;
    stats.num_entries = num_entries_;
    stats.num_buckets = buckets_.size();
    stats.load_factor = static_cast<double>(num_entries_) / buckets_.size();
    
    // Calculate collisions and max chain length
    stats.num_collisions = 0;
    stats.max_chain_length = 0;
    
    for (const auto& chain : buckets_) {
        if (chain.size() > 1) {
            stats.num_collisions += (chain.size() - 1);
        }
        stats.max_chain_length = std::max(stats.max_chain_length, chain.size());
    }
    
    return stats;
}

void HashTable::evict_if_needed() {
    bool should_evict = (eviction_policy_ == EvictionPolicy::LRU) 
                        ? lru_policy_->should_evict() 
                        : lfu_policy_->should_evict();
    
    while (should_evict) {
        std::string victim = (eviction_policy_ == EvictionPolicy::LRU)
                             ? lru_policy_->get_eviction_candidate()
                             : lfu_policy_->get_eviction_candidate();
        
        if (victim.empty()) {
            break; // No more candidates
        }
        
        // Remove the victim
        remove(victim);
        
        // Re-check if more eviction is needed
        should_evict = (eviction_policy_ == EvictionPolicy::LRU)
                       ? lru_policy_->should_evict()
                       : lfu_policy_->should_evict();
    }
}

HashTable::~HashTable() {
    stop_ttl_scanner();
}

void HashTable::start_ttl_scanner(uint64_t scan_interval_ms) {
    scan_interval_ms_ = scan_interval_ms;
    stop_scanner_ = false;
    
    ttl_scanner_thread_ = std::make_unique<std::thread>([this]() {
        while (!stop_scanner_) {
            scan_expired_keys();
            std::this_thread::sleep_for(std::chrono::milliseconds(scan_interval_ms_));
        }
    });
}

void HashTable::stop_ttl_scanner() {
    if (ttl_scanner_thread_ && ttl_scanner_thread_->joinable()) {
        stop_scanner_ = true;
        ttl_scanner_thread_->join();
    }
}

bool HashTable::is_expired(const ValueEntry& entry) const {
    if (entry.expiry_ns == 0) {
        return false; // No expiry set
    }
    
    auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    
    return static_cast<uint64_t>(now) >= entry.expiry_ns;
}

void HashTable::scan_expired_keys() {
    // Sample random keys from random buckets (Redis-style approach)
    // Check ~20 random keys per scan
    constexpr size_t SAMPLE_SIZE = 20;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> bucket_dist(0, buckets_.size() - 1);
    
    std::vector<std::string> expired_keys;
    
    for (size_t i = 0; i < SAMPLE_SIZE; ++i) {
        size_t bucket_idx = bucket_dist(gen);
        Chain& chain = buckets_[bucket_idx];
        
        if (chain.empty()) {
            continue;
        }
        
        // Check all keys in this bucket
        for (const auto& kv : chain) {
            if (is_expired(kv.value)) {
                expired_keys.push_back(kv.key);
            }
        }
    }
    
    // Remove expired keys
    for (const auto& key : expired_keys) {
        remove(key);
    }
}

// WAL Management

void HashTable::enable_wal(const std::string& wal_path, size_t batch_size) {
    wal_path_ = wal_path;
    wal_writer_ = std::make_unique<WALWriter>(wal_path, batch_size);
}

void HashTable::flush_wal() {
    if (wal_writer_) {
        wal_writer_->flush();
    }
}

size_t HashTable::wal_size() const {
    return wal_writer_ ? wal_writer_->size() : 0;
}

size_t HashTable::replay_wal(const std::string& wal_path) {
    WALReader reader(wal_path);
    size_t replayed = 0;
    
    WALRecord record(WALOpType::SET, "");
    while (reader.read_next(record)) {
        switch (record.op_type) {
            case WALOpType::SET: {
                ValueEntry entry(record.value, record.ttl_ns);
                insert(record.key, entry);
                break;
            }
            case WALOpType::DELETE: {
                remove(record.key);
                break;
            }
            case WALOpType::EXPIRE: {
                // EXPIRE sets a new TTL on an existing key
                auto existing = get(record.key);
                if (existing) {
                    ValueEntry updated = **existing;
                    updated.expiry_ns = record.ttl_ns;
                    insert(record.key, updated);
                }
                break;
            }
        }
        replayed++;
    }
    
    return replayed;
}


// Snapshot Management

bool HashTable::create_snapshot(const std::string& snapshot_path) {
    SnapshotWriter writer(snapshot_path);
    return writer.write(*this);
}

bool HashTable::load_snapshot(const std::string& snapshot_path) {
    SnapshotReader reader(snapshot_path);
    bool success = reader.read(*this);
    
    if (success && !reader.verify_checksum()) {
        // CRC mismatch - snapshot corrupted
        return false;
    }
    
    return success;
}

void HashTable::truncate_wal() {
    if (!wal_path_.empty()) {
        // Close existing WAL writer
        wal_writer_.reset();
        
        // Truncate file by reopening in trunc mode
        std::ofstream truncate_file(wal_path_, std::ios::binary | std::ios::trunc);
        truncate_file.close();
        
        // Re-enable WAL with fresh file
        enable_wal(wal_path_);
    }
}
} // namespace kvstore
