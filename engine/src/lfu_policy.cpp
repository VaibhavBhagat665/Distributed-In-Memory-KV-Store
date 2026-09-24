#include "lfu_policy.h"

namespace kvstore {

LFUPolicy::LFUPolicy(size_t memory_limit)
    : memory_limit_(memory_limit), current_memory_(0) {}

void LFUPolicy::on_access(const std::string& key, uint64_t access_count) {
    auto it = key_to_count_.find(key);
    if (it == key_to_count_.end()) {
        return; // Key not tracked (shouldn't happen)
    }
    
    // Update the access count
    it->second = access_count;
    
    // Push updated entry to heap (old entry will be filtered out via lazy deletion)
    size_t value_size = key_to_size_[key];
    heap_.push({key, access_count, value_size});
}

void LFUPolicy::on_insert(const std::string& key, size_t value_size, uint64_t access_count) {
    // Track the key and its metadata
    key_to_count_[key] = access_count;
    key_to_size_[key] = value_size;
    
    // Add to heap
    heap_.push({key, access_count, value_size});
    
    current_memory_ += value_size;
}

void LFUPolicy::on_delete(const std::string& key, size_t value_size) {
    auto it = key_to_count_.find(key);
    if (it == key_to_count_.end()) {
        return; // Key not tracked
    }
    
    // Remove from tracking (heap entries will be lazily filtered)
    key_to_count_.erase(it);
    key_to_size_.erase(key);
    
    if (current_memory_ >= value_size) {
        current_memory_ -= value_size;
    } else {
        current_memory_ = 0;
    }
}

std::string LFUPolicy::get_eviction_candidate() {
    // Pop stale entries from heap (lazy deletion)
    while (!heap_.empty()) {
        Entry top = heap_.top();
        heap_.pop();
        
        // Check if this entry is still valid
        auto it = key_to_count_.find(top.key);
        if (it != key_to_count_.end() && it->second == top.access_count) {
            // Valid entry found - this is our eviction candidate
            return top.key;
        }
        // Entry is stale (deleted or updated), continue to next
    }
    
    return ""; // No valid entries
}

bool LFUPolicy::should_evict() const {
    return current_memory_ > memory_limit_;
}

} // namespace kvstore
