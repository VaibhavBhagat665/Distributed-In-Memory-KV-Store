#ifndef KVSTORE_ENGINE_C_H
#define KVSTORE_ENGINE_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Opaque handle to C++ engine
typedef struct EngineHandle EngineHandle;

// Create engine with specified number of threads and memory limit
EngineHandle* engine_create(int num_threads, size_t mem_limit_bytes);

// Destroy engine and free resources
void engine_destroy(EngineHandle* handle);

// Set key-value pair with optional TTL (0 = no expiry)
// Returns: 0 on success, -1 on error, -2 if value too large
int engine_set(EngineHandle* handle, const char* key, size_t klen,
               const char* val, size_t vlen, int64_t ttl_ns);

// Get value for key
// Returns: 0 on success, -1 if not found, -2 if buffer too small
// out_len: input = buffer size, output = actual value size
int engine_get(EngineHandle* handle, const char* key, size_t klen,
               char* out_buf, size_t* out_len);

// Delete key
// Returns: 0 on success, -1 if not found
int engine_delete(EngineHandle* handle, const char* key, size_t klen);

// Replay WAL from directory
// Returns: number of operations replayed, or -1 on error
int engine_replay_wal(EngineHandle* handle, const char* wal_dir);

// Create snapshot
// Returns: 0 on success, -1 on error
int engine_snapshot(EngineHandle* handle, const char* snapshot_path);

#ifdef __cplusplus
}
#endif

#endif // KVSTORE_ENGINE_C_H
