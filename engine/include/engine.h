#ifndef KVSTORE_ENGINE_H
#define KVSTORE_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the storage engine
typedef struct EngineHandle EngineHandle;

// Lifecycle functions

/**
 * Create a new storage engine instance.
 * 
 * @param num_threads Number of worker threads (shards)
 * @param mem_limit_bytes Maximum memory usage in bytes
 * @return Opaque handle to the engine, or NULL on failure
 */
EngineHandle* engine_create(int num_threads, size_t mem_limit_bytes);

/**
 * Destroy the storage engine and free all resources.
 * 
 * @param handle Engine handle
 */
void engine_destroy(EngineHandle* handle);

// Core operations

/**
 * Set a key-value pair with optional TTL.
 * 
 * @param handle Engine handle
 * @param key Key string
 * @param klen Key length
 * @param val Value string
 * @param vlen Value length
 * @param ttl_ns TTL in nanoseconds (0 = no expiry)
 * @return 0 on success, negative error code on failure
 */
int engine_set(EngineHandle* handle, const char* key, size_t klen,
               const char* val, size_t vlen, int64_t ttl_ns);

/**
 * Get a value by key.
 * 
 * @param handle Engine handle
 * @param key Key string
 * @param klen Key length
 * @param out_buf Output buffer for value (allocated by caller)
 * @param out_len Input: buffer size, Output: actual value length
 * @return 0 on success, -1 if key not found, -2 if buffer too small
 */
int engine_get(EngineHandle* handle, const char* key, size_t klen,
               char* out_buf, size_t* out_len);

/**
 * Delete a key.
 * 
 * @param handle Engine handle
 * @param key Key string
 * @param klen Key length
 * @return 0 on success, -1 if key not found
 */
int engine_delete(EngineHandle* handle, const char* key, size_t klen);

// Persistence functions

/**
 * Replay write-ahead log to restore state.
 * 
 * @param handle Engine handle
 * @param wal_dir Directory containing WAL files
 * @return 0 on success, negative error code on failure
 */
int engine_replay_wal(EngineHandle* handle, const char* wal_dir);

/**
 * Create a snapshot of current state.
 * 
 * @param handle Engine handle
 * @param snapshot_path Path to snapshot file
 * @return 0 on success, negative error code on failure
 */
int engine_snapshot(EngineHandle* handle, const char* snapshot_path);

#ifdef __cplusplus
}
#endif

#endif // KVSTORE_ENGINE_H
