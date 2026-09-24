#include "snapshot.h"
#include "hash_table.h"
#include <sys/stat.h>
#include <cstring>

namespace kvstore {

// CRC64 implementation (ECMA-182 polynomial 0x42F0E1EBA9EA3693)
uint64_t CRC64::table_[256];
bool CRC64::table_initialized_ = false;

void CRC64::init_table() {
    constexpr uint64_t poly = 0x42F0E1EBA9EA3693ULL;
    
    for (uint32_t i = 0; i < 256; ++i) {
        uint64_t crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ poly;
            } else {
                crc >>= 1;
            }
        }
        table_[i] = crc;
    }
    table_initialized_ = true;
}

CRC64::CRC64() : crc_(0xFFFFFFFFFFFFFFFFULL) {
    if (!table_initialized_) {
        init_table();
    }
}

void CRC64::update(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        uint8_t idx = (crc_ ^ bytes[i]) & 0xFF;
        crc_ = (crc_ >> 8) ^ table_[idx];
    }
}

// SnapshotWriter implementation

SnapshotWriter::SnapshotWriter(const std::string& filepath)
    : filepath_(filepath) {
    file_.open(filepath_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open snapshot file for writing: " + filepath_);
    }
}

SnapshotWriter::~SnapshotWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool SnapshotWriter::write(HashTable& ht) {
    crc_.reset();
    
    // Write magic number
    file_.write(reinterpret_cast<const char*>(&SNAPSHOT_MAGIC), sizeof(SNAPSHOT_MAGIC));
    crc_.update(&SNAPSHOT_MAGIC, sizeof(SNAPSHOT_MAGIC));
    
    // Placeholder for CRC64 (will be filled later)
    uint64_t placeholder_crc = 0;
    std::streampos crc_pos = file_.tellp();
    file_.write(reinterpret_cast<const char*>(&placeholder_crc), sizeof(placeholder_crc));
    
    // Write number of entries
    size_t num_entries = ht.size();
    file_.write(reinterpret_cast<const char*>(&num_entries), sizeof(num_entries));
    crc_.update(&num_entries, sizeof(num_entries));
    
    // Write all entries
    ht.for_each([this](const std::string& key, const ValueEntry& value) {
        write_entry(key, value);
    });
    
    // Go back and write CRC
    uint64_t final_crc = crc_.finalize();
    file_.seekp(crc_pos);
    file_.write(reinterpret_cast<const char*>(&final_crc), sizeof(final_crc));
    
    file_.flush();
    return file_.good();
}

void SnapshotWriter::write_entry(const std::string& key, const ValueEntry& value) {
    // key_len
    uint32_t key_len = key.size();
    file_.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    crc_.update(&key_len, sizeof(key_len));
    
    // key
    file_.write(key.data(), key_len);
    crc_.update(key.data(), key_len);
    
    // value_len
    uint32_t value_len = value.value.size();
    file_.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
    crc_.update(&value_len, sizeof(value_len));
    
    // value
    file_.write(value.value.data(), value_len);
    crc_.update(value.value.data(), value_len);
    
    // ttl
    file_.write(reinterpret_cast<const char*>(&value.expiry_ns), sizeof(value.expiry_ns));
    crc_.update(&value.expiry_ns, sizeof(value.expiry_ns));
    
    // access_count
    file_.write(reinterpret_cast<const char*>(&value.access_count), sizeof(value.access_count));
    crc_.update(&value.access_count, sizeof(value.access_count));
    
    // last_access_ns
    file_.write(reinterpret_cast<const char*>(&value.last_access_ns), sizeof(value.last_access_ns));
    crc_.update(&value.last_access_ns, sizeof(value.last_access_ns));
}

size_t SnapshotWriter::size() const {
    struct stat st;
    if (stat(filepath_.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

// SnapshotReader implementation

SnapshotReader::SnapshotReader(const std::string& filepath)
    : filepath_(filepath), stored_crc_(0) {
    file_.open(filepath_, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open snapshot file for reading: " + filepath_);
    }
}

SnapshotReader::~SnapshotReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool SnapshotReader::read(HashTable& ht) {
    computed_crc_.reset();
    
    // Read magic number
    uint64_t magic;
    file_.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != SNAPSHOT_MAGIC) {
        return false;  // Invalid snapshot file
    }
    computed_crc_.update(&magic, sizeof(magic));
    
    // Read stored CRC
    file_.read(reinterpret_cast<char*>(&stored_crc_), sizeof(stored_crc_));
    // Don't include CRC in CRC computation
    
    // Read number of entries
    size_t num_entries;
    file_.read(reinterpret_cast<char*>(&num_entries), sizeof(num_entries));
    computed_crc_.update(&num_entries, sizeof(num_entries));
    
    // Read all entries
    for (size_t i = 0; i < num_entries; ++i) {
        std::string key;
        ValueEntry value("", 0);
        
        if (!read_entry(key, value)) {
            return false;  // Corrupted snapshot
        }
        
        ht.insert(key, value);
    }
    
    return true;
}

bool SnapshotReader::read_entry(std::string& key, ValueEntry& value) {
    // key_len
    uint32_t key_len;
    file_.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
    if (file_.gcount() != sizeof(key_len)) return false;
    computed_crc_.update(&key_len, sizeof(key_len));
    
    // key
    key.resize(key_len);
    file_.read(&key[0], key_len);
    if (static_cast<uint32_t>(file_.gcount()) != key_len) return false;
    computed_crc_.update(key.data(), key_len);
    
    // value_len
    uint32_t value_len;
    file_.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));
    if (file_.gcount() != sizeof(value_len)) return false;
    computed_crc_.update(&value_len, sizeof(value_len));
    
    // value
    value.value.resize(value_len);
    file_.read(&value.value[0], value_len);
    if (static_cast<uint32_t>(file_.gcount()) != value_len) return false;
    computed_crc_.update(value.value.data(), value_len);
    
    // ttl
    file_.read(reinterpret_cast<char*>(&value.expiry_ns), sizeof(value.expiry_ns));
    if (file_.gcount() != sizeof(value.expiry_ns)) return false;
    computed_crc_.update(&value.expiry_ns, sizeof(value.expiry_ns));
    
    // access_count
    file_.read(reinterpret_cast<char*>(&value.access_count), sizeof(value.access_count));
    if (file_.gcount() != sizeof(value.access_count)) return false;
    computed_crc_.update(&value.access_count, sizeof(value.access_count));
    
    // last_access_ns
    file_.read(reinterpret_cast<char*>(&value.last_access_ns), sizeof(value.last_access_ns));
    if (file_.gcount() != sizeof(value.last_access_ns)) return false;
    computed_crc_.update(&value.last_access_ns, sizeof(value.last_access_ns));
    
    return true;
}

bool SnapshotReader::verify_checksum() {
    return computed_crc_.finalize() == stored_crc_;
}

} // namespace kvstore
