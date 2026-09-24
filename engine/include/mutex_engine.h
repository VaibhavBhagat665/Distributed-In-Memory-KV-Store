#ifndef KVSTORE_MUTEX_ENGINE_H
#define KVSTORE_MUTEX_ENGINE_H

#include "hash_table.h"
#include <mutex>
#include <optional>

namespace kvstore {

// Global-mutex baseline for comparison with shared-nothing design
// Uses a single mutex to protect all operations on a single HashTable
// Expected to show poor scaling due to lock contention
class MutexEngine {
public:
    MutexEngine() : table_() {}
    
    bool set(const std::string& key, const ValueEntry& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        return table_.insert(key, value);
    }
    
    std::optional<ValueEntry*> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return table_.get(key);
    }
    
    bool remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return table_.remove(key);
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return table_.size();
    }

private:
    mutable std::mutex mutex_;  // Single global mutex for all operations
    HashTable table_;
};

}  // namespace kvstore

#endif  // KVSTORE_MUTEX_ENGINE_H
