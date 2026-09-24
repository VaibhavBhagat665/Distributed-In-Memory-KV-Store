#ifndef KVSTORE_LRU_POLICY_H
#define KVSTORE_LRU_POLICY_H

#include <string>
#include <list>
#include <unordered_map>
#include <cstdint>

namespace kvstore {

/**
 * LRU (Least Recently Used) eviction policy.
 * 
 * Maintains a doubly-linked list ordered by access time.
 * Most recently accessed items are at the front.
 * Least recently accessed items (candidates for eviction) are at the back.
 * 
 * O(1) access update via hash map to list iterators.
 */
class LRUPolicy {
public:
    LRUPolicy(size_t memory_limit);
    
    // Record an access to a key
    void on_access(const std::string& key, uint64_t last_access_ns);
    
    // Record insertion of a new key
    void on_insert(const std::string& key, size_t value_size, uint64_t last_access_ns);
    
    // Record deletion of a key
    void on_delete(const std::string& key, size_t value_size);
    
    // Get key to evict (returns empty string if no eviction needed)
    std::string get_eviction_candidate();
    
    // Check if eviction is needed
    bool should_evict() const;
    
    // Get current memory usage
    size_t current_memory() const { return current_memory_; }
    
private:
    struct Entry {
        std::string key;
        uint64_t last_access_ns;
        size_t value_size;
    };
    
    size_t memory_limit_;
    size_t current_memory_;
    
    // List ordered by access time (front = most recent, back = least recent)
    std::list<Entry> access_list_;
    
    // Map from key to iterator in access_list for O(1) updates
    std::unordered_map<std::string, std::list<Entry>::iterator> key_to_iter_;
};

} // namespace kvstore

#endif // KVSTORE_LRU_POLICY_H
