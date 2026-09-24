#ifndef KVSTORE_LFU_POLICY_H
#define KVSTORE_LFU_POLICY_H

#include <string>
#include <unordered_map>
#include <queue>
#include <vector>
#include <cstdint>

namespace kvstore {

/**
 * LFU (Least Frequently Used) eviction policy.
 * 
 * Maintains a min-heap ordered by access count.
 * Entries with lowest access count are evicted first.
 * 
 * Uses lazy deletion: entries may remain in heap after deletion,
 * but are filtered out when popping from heap.
 */
class LFUPolicy {
public:
    LFUPolicy(size_t memory_limit);
    
    // Record an access to a key
    void on_access(const std::string& key, uint64_t access_count);
    
    // Record insertion of a new key
    void on_insert(const std::string& key, size_t value_size, uint64_t access_count);
    
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
        uint64_t access_count;
        size_t value_size;
        
        // For min-heap: smaller access_count = higher priority for eviction
        bool operator>(const Entry& other) const {
            return access_count > other.access_count;
        }
    };
    
    size_t memory_limit_;
    size_t current_memory_;
    
    // Min-heap: least frequently used at top
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap_;
    
    // Track which keys are currently valid (for lazy deletion)
    std::unordered_map<std::string, uint64_t> key_to_count_;
    
    // Track value sizes for memory accounting
    std::unordered_map<std::string, size_t> key_to_size_;
};

} // namespace kvstore

#endif // KVSTORE_LFU_POLICY_H
