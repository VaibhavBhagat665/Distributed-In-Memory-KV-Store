#include "wal.h"
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>

namespace kvstore {

// WALWriter implementation

WALWriter::WALWriter(const std::string& filepath, size_t batch_size)
    : filepath_(filepath), batch_size_(batch_size), file_size_(0) {
    
    // Open file in append mode, create if doesn't exist
    file_.open(filepath_, std::ios::binary | std::ios::app | std::ios::out);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open WAL file: " + filepath_);
    }
    
    // Get current file size
    struct stat st;
    if (stat(filepath_.c_str(), &st) == 0) {
        file_size_ = st.st_size;
    }
    
    buffer_.reserve(batch_size_);
}

WALWriter::~WALWriter() {
    if (file_.is_open()) {
        try {
            flush();
        } catch (...) {
            // Suppress exceptions in destructor
        }
        file_.close();
    }
}

void WALWriter::append(const WALRecord& record) {
    buffer_.push_back(record);
    
    // Auto-flush when batch is full
    if (buffer_.size() >= batch_size_) {
        flush();
    }
}

void WALWriter::flush() {
    if (buffer_.empty()) {
        return;
    }
    
    // Write all buffered records
    for (const auto& record : buffer_) {
        write_record(record);
    }
    
    // Flush to OS buffer
    file_.flush();
    
    // Force fsync to disk
    fsync_file();
    
    buffer_.clear();
}

void WALWriter::write_record(const WALRecord& record) {
    // Write op_type (1 byte)
    uint8_t op = static_cast<uint8_t>(record.op_type);
    file_.write(reinterpret_cast<const char*>(&op), sizeof(op));
    
    // Write key_len (4 bytes)
    uint32_t key_len = record.key.size();
    file_.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    
    // Write key
    file_.write(record.key.data(), key_len);
    
    // Write value_len (4 bytes)
    uint32_t value_len = record.value.size();
    file_.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
    
    // Write value
    if (value_len > 0) {
        file_.write(record.value.data(), value_len);
    }
    
    // Write ttl (8 bytes)
    file_.write(reinterpret_cast<const char*>(&record.ttl_ns), sizeof(record.ttl_ns));
    
    // Update file size
    file_size_ += 1 + 4 + key_len + 4 + value_len + 8;
}

void WALWriter::fsync_file() {
    // Get file descriptor and call fsync
    int fd = -1;
    
    // Platform-specific way to get file descriptor from fstream
    #ifdef _WIN32
    // Windows doesn't easily expose fd from fstream, just flush
    file_.flush();
    #else
    // Unix/Linux: use file descriptor
    FILE* cfile = fopen(filepath_.c_str(), "r");
    if (cfile) {
        fd = fileno(cfile);
        if (fd >= 0) {
            ::fsync(fd);
        }
        fclose(cfile);
    }
    #endif
}

size_t WALWriter::size() const {
    return file_size_;
}

// WALReader implementation

WALReader::WALReader(const std::string& filepath)
    : filepath_(filepath), records_read_(0) {
    
    file_.open(filepath_, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open WAL file for reading: " + filepath_);
    }
}

WALReader::~WALReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool WALReader::read_next(WALRecord& record) {
    if (!file_.good() || file_.eof()) {
        return false;
    }
    
    // Read op_type (1 byte)
    uint8_t op;
    file_.read(reinterpret_cast<char*>(&op), sizeof(op));
    if (file_.gcount() != sizeof(op)) {
        return false;  // Incomplete record or EOF
    }
    record.op_type = static_cast<WALOpType>(op);
    
    // Read key_len (4 bytes)
    uint32_t key_len;
    file_.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
    if (file_.gcount() != sizeof(key_len)) {
        return false;
    }
    
    // Read key
    record.key.resize(key_len);
    file_.read(&record.key[0], key_len);
    if (static_cast<uint32_t>(file_.gcount()) != key_len) {
        return false;
    }
    
    // Read value_len (4 bytes)
    uint32_t value_len;
    file_.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
    if (file_.gcount() != sizeof(value_len)) {
        return false;
    }
    
    // Read value
    if (value_len > 0) {
        record.value.resize(value_len);
        file_.read(&record.value[0], value_len);
        if (static_cast<uint32_t>(file_.gcount()) != value_len) {
            return false;
        }
    } else {
        record.value.clear();
    }
    
    // Read ttl (8 bytes)
    file_.read(reinterpret_cast<char*>(&record.ttl_ns), sizeof(record.ttl_ns));
    if (file_.gcount() != sizeof(record.ttl_ns)) {
        return false;
    }
    
    records_read_++;
    return true;
}

bool WALReader::read_string(std::string& str) {
    uint32_t len;
    file_.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (file_.gcount() != sizeof(len)) {
        return false;
    }
    
    str.resize(len);
    file_.read(&str[0], len);
    return static_cast<uint32_t>(file_.gcount()) == len;
}

} // namespace kvstore
