#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace kvstore {

// RESP data types
// See: https://redis.io/docs/reference/protocol-spec/
enum class RESPType {
    SimpleString,  // +OK\r\n
    Error,         // -Error message\r\n
    Integer,       // :1000\r\n
    BulkString,    // $6\r\nfoobar\r\n
    Array,         // *2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
    NullBulkString // $-1\r\n
};

// RESP value - variant holding different types
class RESPValue {
public:
    RESPType type;
    std::string string_value;  // For SimpleString, Error, BulkString
    int64_t int_value;         // For Integer
    std::vector<RESPValue> array_value;  // For Array
    
    // Constructors
    static RESPValue simple_string(const std::string& s);
    static RESPValue error(const std::string& msg);
    static RESPValue integer(int64_t i);
    static RESPValue bulk_string(const std::string& s);
    static RESPValue null_bulk_string();
    static RESPValue array(const std::vector<RESPValue>& arr);
    
    // Serialize to RESP protocol format
    std::string serialize() const;
    
private:
    RESPValue(RESPType t) : type(t), int_value(0) {}
};

// RESP command after parsing
struct RESPCommand {
    std::string command;  // GET, SET, DELETE, EXPIRE, etc.
    std::vector<std::string> args;
    
    RESPCommand(const std::string& cmd, const std::vector<std::string>& arguments)
        : command(cmd), args(arguments) {}
};

// RESP parser - stateful parser for incoming bytes
class RESPParser {
public:
    RESPParser();
    
    // Feed bytes to parser, returns parsed value if complete
    std::optional<RESPValue> parse(const char* data, size_t len);
    
    // Convert RESPValue array to command
    static std::optional<RESPCommand> to_command(const RESPValue& value);
    
    // Reset parser state
    void reset();
    
    // Check if parser is in clean state (not mid-parse)
    bool is_clean() const { return state_ == State::Start; }
    
private:
    enum class State {
        Start,
        SimpleString,
        Error,
        Integer,
        BulkStringLen,
        BulkStringData,
        ArrayLen,
        ArrayElement
    };
    
    State state_;
    std::string buffer_;
    
    // For bulk strings
    int64_t bulk_len_;
    size_t bulk_read_;
    
    // For arrays
    int64_t array_len_;
    std::vector<RESPValue> array_elements_;
    
    // Helper: try to read a line ending with \r\n
    std::optional<std::string> read_line();
    
    // Helper: parse integer from buffer
    std::optional<int64_t> parse_integer(const std::string& s);
};

// RESP formatter - convenience functions for common responses
class RESPFormatter {
public:
    static std::string ok();
    static std::string error(const std::string& msg);
    static std::string null();
    static std::string string(const std::string& s);
    static std::string integer(int64_t i);
    static std::string array(const std::vector<std::string>& strings);
};

} // namespace kvstore
