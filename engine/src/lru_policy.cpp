#include "lru_policy.h"

namespace kvstore {

LRUPolicy::LRUPolicy(size_t memory_limit)
    : memory_limit_(memory_limit), current_memory_(0) {}

void LRUPolicy::on_access(const std::string& key, uint64_t last_access_ns) {
    auto it = key_to_iter_.find(key);
    if (it == key_to_iter_.end()) {
        return; // Key not tracked (shouldn't happen)
    }
    
    // Move to front (most recently used)
    Entry entry = *it->second;
    entry.last_access_ns = last_access_ns;
    
    access_list_.erase(it->second);
    access_list_.push_front(entry);
    key_to_iter_[key] = access_list_.begin();
}

void LRUPolicy::on_insert(const std::string& key, size_t value_size, uint64_t last_access_ns) {
    // Add to front of list (most recently used)
    Entry entry{key, last_access_ns, value_size};
    access_list_.push_front(entry);
    key_to_iter_[key] = access_list_.begin();
    
    current_memory_ += value_size;
}

void LRUPolicy::on_delete(const std::string& key, size_t value_size) {
    auto it = key_to_iter_.find(key);
    if (it == key_to_iter_.end()) {
        return; // Key not tracked
    }
    
    access_list_.erase(it->second);
    key_to_iter_.erase(it);
    
    if (current_memory_ >= value_size) {
        current_memory_ -= value_size;
    } else {
        current_memory_ = 0;
    }
}

std::string LRUPolicy::get_eviction_candidate() {
    if (access_list_.empty()) {
        return "";
    }
    
    // Return least recently used (back of list)
    return access_list_.back().key;
}

bool LRUPolicy::should_evict() const {
    return current_memory_ > memory_limit_;
}

} // namespace kvstore
