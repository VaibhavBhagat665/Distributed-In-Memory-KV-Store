#include "engine.h"
#include "hash_table.h"
#include <cstdlib>
#include <cstring>
#include <chrono>

// Implementation using the hash table

struct EngineHandle {
    int num_threads;
    size_t mem_limit_bytes;
    kvstore::HashTable* table;  // Single-threaded for now, will add sharding later
};

EngineHandle* engine_create(int num_threads, size_t mem_limit_bytes) {
    if (num_threads <= 0 || mem_limit_bytes == 0) {
        return nullptr;
    }
    
    EngineHandle* handle = new EngineHandle;
    handle->num_threads = num_threads;
    handle->mem_limit_bytes = mem_limit_bytes;
    handle->table = new kvstore::HashTable();
    
    return handle;
}

void engine_destroy(EngineHandle* handle) {
    if (handle) {
        delete handle->table;
        delete handle;
    }
}

int engine_set(EngineHandle* handle, const char* key, size_t klen,
               const char* val, size_t vlen, int64_t ttl_ns) {
    if (!handle || !key || !val) {
        return -1;
    }
    
    // Check value size limit (512KB)
    if (vlen > 512 * 1024) {
        return -2; // Value too large
    }
    
    std::string key_str(key, klen);
    std::string val_str(val, vlen);
    
    // Convert relative TTL to absolute expiry time
    uint64_t expiry_ns = 0;
    if (ttl_ns > 0) {
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        expiry_ns = static_cast<uint64_t>(now) + static_cast<uint64_t>(ttl_ns);
    }
    
    kvstore::ValueEntry value(val_str, expiry_ns);
    handle->table->insert(key_str, value);
    
    return 0;
}

int engine_get(EngineHandle* handle, const char* key, size_t klen,
               char* out_buf, size_t* out_len) {
    if (!handle || !key || !out_buf || !out_len) {
        return -1;
    }
    
    std::string key_str(key, klen);
    auto result = handle->table->get(key_str);
    
    if (!result) {
        return -1; // Not found
    }
    
    kvstore::ValueEntry* entry = *result;
    
    // Check if expired (lazy expiry)
    if (entry->expiry_ns > 0) {
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        
        if (static_cast<uint64_t>(now) >= entry->expiry_ns) {
            // Expired - remove it and return not found
            handle->table->remove(key_str);
            return -1;
        }
    }
    
    // Check if buffer is large enough
    if (entry->value.size() > *out_len) {
        *out_len = entry->value.size();
        return -2; // Buffer too small
    }
    
    // Copy value to output buffer
    std::memcpy(out_buf, entry->value.data(), entry->value.size());
    *out_len = entry->value.size();
    
    return 0;
}

int engine_delete(EngineHandle* handle, const char* key, size_t klen) {
    if (!handle || !key) {
        return -1;
    }
    
    std::string key_str(key, klen);
    bool removed = handle->table->remove(key_str);
    
    return removed ? 0 : -1;
}

int engine_replay_wal(EngineHandle* handle, const char* wal_dir) {
    // Placeholder - will implement with WAL
    (void)handle; (void)wal_dir;
    return -1; // Not implemented yet
}

int engine_snapshot(EngineHandle* handle, const char* snapshot_path) {
    // Placeholder - will implement with snapshot manager
    (void)handle; (void)snapshot_path;
    return -1; // Not implemented yet
}
