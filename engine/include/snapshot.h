#pragma once

#include <string>
#include <fstream>
#include <cstdint>

namespace kvstore {

// Forward declarations
class HashTable;
struct ValueEntry;

// Snapshot file format:
// [MAGIC:8]['KVSNAP01'][CRC64:8][num_entries:8][entries...]
// Each entry: [key_len:4][key:var][value_len:4][value:var][ttl:8][access_count:8][last_access_ns:8]

constexpr uint64_t SNAPSHOT_MAGIC = 0x4B5653'4E415030'31ULL;  // 'KVSNAPOI' in hex
constexpr size_t SNAPSHOT_HEADER_SIZE = 8 + 8;  // magic + crc64

// CRC64 computation (ECMA-182 polynomial)
class CRC64 {
public:
    CRC64();
    void update(const void* data, size_t len);
    uint64_t finalize() const { return crc_; }
    void reset() { crc_ = 0xFFFFFFFFFFFFFFFFULL; }
    
private:
    uint64_t crc_;
    static uint64_t table_[256];
    static bool table_initialized_;
    static void init_table();
};

// Snapshot writer - serializes HashTable to disk
class SnapshotWriter {
public:
    explicit SnapshotWriter(const std::string& filepath);
    ~SnapshotWriter();
    
    // Write snapshot from HashTable
    // Uses copy-on-write approach: quickly captures state, then writes in background
    bool write(HashTable& ht);
    
    // Get file size
    size_t size() const;
    
private:
    std::string filepath_;
    std::ofstream file_;
    CRC64 crc_;
    
    void write_header(size_t num_entries);
    void write_entry(const std::string& key, const ValueEntry& value);
    void finalize_crc();
};

// Snapshot reader - loads HashTable from disk
class SnapshotReader {
public:
    explicit SnapshotReader(const std::string& filepath);
    ~SnapshotReader();
    
    // Read snapshot into HashTable
    bool read(HashTable& ht);
    
    // Verify CRC64 checksum
    bool verify_checksum();
    
    bool is_valid() const { return file_.good(); }
    
private:
    std::string filepath_;
    std::ifstream file_;
    uint64_t stored_crc_;
    CRC64 computed_crc_;
    
    bool read_header(size_t& num_entries);
    bool read_entry(std::string& key, ValueEntry& value);
};

} // namespace kvstore
