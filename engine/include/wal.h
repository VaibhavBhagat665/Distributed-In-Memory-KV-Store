#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

namespace kvstore {

// WAL operation types
enum class WALOpType : uint8_t {
    SET = 1,
    DELETE = 2,
    EXPIRE = 3
};

// WAL record structure (not written directly, used for parsing)
struct WALRecord {
    WALOpType op_type;
    std::string key;
    std::string value;  // empty for DELETE
    int64_t ttl_ns;     // 0 for no TTL, -1 for EXPIRE with absolute timestamp
    
    WALRecord(WALOpType op, const std::string& k, const std::string& v = "", int64_t ttl = 0)
        : op_type(op), key(k), value(v), ttl_ns(ttl) {}
};

// Write-Ahead Log writer
// Binary format: [op_type:1][key_len:4][key:var][value_len:4][value:var][ttl:8]
class WALWriter {
public:
    WALWriter(const std::string& filepath, size_t batch_size = 100);
    ~WALWriter();
    
    // Append operation to WAL (buffered)
    void append(const WALRecord& record);
    
    // Force flush to disk with fsync
    void flush();
    
    // Get current WAL file size
    size_t size() const;
    
    // Get number of pending operations in buffer
    size_t pending_count() const { return buffer_.size(); }

private:
    std::string filepath_;
    std::ofstream file_;
    size_t batch_size_;
    std::vector<WALRecord> buffer_;
    size_t file_size_;
    
    void write_record(const WALRecord& record);
    void fsync_file();
};

// Write-Ahead Log reader for recovery
class WALReader {
public:
    explicit WALReader(const std::string& filepath);
    ~WALReader();
    
    // Read next record (returns false at EOF or on error)
    bool read_next(WALRecord& record);
    
    // Check if reader is in valid state
    bool is_valid() const { return file_.good(); }
    
    // Get total records read
    size_t records_read() const { return records_read_; }

private:
    std::string filepath_;
    std::ifstream file_;
    size_t records_read_;
    
    bool read_string(std::string& str);
};

} // namespace kvstore
