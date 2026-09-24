#include "resp.h"
#include <algorithm>
#include <sstream>

namespace kvstore {

// RESPValue constructors

RESPValue RESPValue::simple_string(const std::string& s) {
    RESPValue v(RESPType::SimpleString);
    v.string_value = s;
    return v;
}

RESPValue RESPValue::error(const std::string& msg) {
    RESPValue v(RESPType::Error);
    v.string_value = msg;
    return v;
}

RESPValue RESPValue::integer(int64_t i) {
    RESPValue v(RESPType::Integer);
    v.int_value = i;
    return v;
}

RESPValue RESPValue::bulk_string(const std::string& s) {
    RESPValue v(RESPType::BulkString);
    v.string_value = s;
    return v;
}

RESPValue RESPValue::null_bulk_string() {
    return RESPValue(RESPType::NullBulkString);
}

RESPValue RESPValue::array(const std::vector<RESPValue>& arr) {
    RESPValue v(RESPType::Array);
    v.array_value = arr;
    return v;
}

// Serialize to RESP format

std::string RESPValue::serialize() const {
    std::ostringstream oss;
    
    switch (type) {
        case RESPType::SimpleString:
            oss << "+" << string_value << "\r\n";
            break;
            
        case RESPType::Error:
            oss << "-" << string_value << "\r\n";
            break;
            
        case RESPType::Integer:
            oss << ":" << int_value << "\r\n";
            break;
            
        case RESPType::BulkString:
            oss << "$" << string_value.size() << "\r\n"
                << string_value << "\r\n";
            break;
            
        case RESPType::NullBulkString:
            oss << "$-1\r\n";
            break;
            
        case RESPType::Array:
            oss << "*" << array_value.size() << "\r\n";
            for (const auto& elem : array_value) {
                oss << elem.serialize();
            }
            break;
    }
    
    return oss.str();
}

// RESPParser implementation

RESPParser::RESPParser()
    : state_(State::Start), bulk_len_(0), bulk_read_(0), array_len_(0) {
}

void RESPParser::reset() {
    state_ = State::Start;
    buffer_.clear();
    bulk_len_ = 0;
    bulk_read_ = 0;
    array_len_ = 0;
    array_elements_.clear();
}

std::optional<std::string> RESPParser::read_line() {
    size_t pos = buffer_.find("\r\n");
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    
    std::string line = buffer_.substr(0, pos);
    buffer_.erase(0, pos + 2);  // Remove line + \r\n
    return line;
}

std::optional<int64_t> RESPParser::parse_integer(const std::string& s) {
    try {
        return std::stoll(s);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<RESPValue> RESPParser::parse(const char* data, size_t len) {
    buffer_.append(data, len);
    
    while (!buffer_.empty()) {
        switch (state_) {
            case State::Start: {
                if (buffer_.empty()) return std::nullopt;
                
                char type_byte = buffer_[0];
                buffer_.erase(0, 1);
                
                switch (type_byte) {
                    case '+':
                        state_ = State::SimpleString;
                        break;
                    case '-':
                        state_ = State::Error;
                        break;
                    case ':':
                        state_ = State::Integer;
                        break;
                    case '$':
                        state_ = State::BulkStringLen;
                        break;
                    case '*':
                        state_ = State::ArrayLen;
                        break;
                    default:
                        // Invalid type byte
                        reset();
                        return std::nullopt;
                }
                break;
            }
            
            case State::SimpleString: {
                auto line = read_line();
                if (!line) return std::nullopt;
                
                reset();
                return RESPValue::simple_string(*line);
            }
            
            case State::Error: {
                auto line = read_line();
                if (!line) return std::nullopt;
                
                reset();
                return RESPValue::error(*line);
            }
            
            case State::Integer: {
                auto line = read_line();
                if (!line) return std::nullopt;
                
                auto num = parse_integer(*line);
                if (!num) {
                    reset();
                    return std::nullopt;
                }
                
                reset();
                return RESPValue::integer(*num);
            }
            
            case State::BulkStringLen: {
                auto line = read_line();
                if (!line) return std::nullopt;
                
                auto len = parse_integer(*line);
                if (!len) {
                    reset();
                    return std::nullopt;
                }
                
                if (*len == -1) {
                    // Null bulk string
                    reset();
                    return RESPValue::null_bulk_string();
                }
                
                bulk_len_ = *len;
                bulk_read_ = 0;
                state_ = State::BulkStringData;
                break;
            }
            
            case State::BulkStringData: {
                size_t needed = bulk_len_ + 2 - bulk_read_;  // +2 for \r\n
                if (buffer_.size() < needed) {
                    return std::nullopt;  // Not enough data yet
                }
                
                std::string data = buffer_.substr(0, bulk_len_);
                buffer_.erase(0, bulk_len_ + 2);  // Remove data + \r\n
                
                reset();
                return RESPValue::bulk_string(data);
            }
            
            case State::ArrayLen: {
                auto line = read_line();
                if (!line) return std::nullopt;
                
                auto len = parse_integer(*line);
                if (!len || *len < 0) {
                    reset();
                    return std::nullopt;
                }
                
                array_len_ = *len;
                array_elements_.clear();
                array_elements_.reserve(array_len_);
                
                if (array_len_ == 0) {
                    reset();
                    return RESPValue::array({});
                }
                
                state_ = State::ArrayElement;
                break;
            }
            
            case State::ArrayElement: {
                // Recursively parse array element
                state_ = State::Start;
                auto elem = parse("", 0);  // Continue parsing from buffer
                
                if (!elem) {
                    state_ = State::ArrayElement;
                    return std::nullopt;  // Need more data
                }
                
                array_elements_.push_back(*elem);
                
                if (array_elements_.size() == static_cast<size_t>(array_len_)) {
                    // Array complete
                    auto result = RESPValue::array(array_elements_);
                    reset();
                    return result;
                }
                
                state_ = State::ArrayElement;
                break;
            }
        }
    }
    
    return std::nullopt;
}

std::optional<RESPCommand> RESPParser::to_command(const RESPValue& value) {
    if (value.type != RESPType::Array) {
        return std::nullopt;
    }
    
    if (value.array_value.empty()) {
        return std::nullopt;
    }
    
    // First element is command
    const auto& cmd_value = value.array_value[0];
    if (cmd_value.type != RESPType::BulkString) {
        return std::nullopt;
    }
    
    // Convert command to uppercase
    std::string command = cmd_value.string_value;
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);
    
    // Remaining elements are arguments
    std::vector<std::string> args;
    for (size_t i = 1; i < value.array_value.size(); ++i) {
        const auto& arg_value = value.array_value[i];
        if (arg_value.type != RESPType::BulkString) {
            return std::nullopt;
        }
        args.push_back(arg_value.string_value);
    }
    
    return RESPCommand(command, args);
}

// RESPFormatter implementation

std::string RESPFormatter::ok() {
    return RESPValue::simple_string("OK").serialize();
}

std::string RESPFormatter::error(const std::string& msg) {
    return RESPValue::error(msg).serialize();
}

std::string RESPFormatter::null() {
    return RESPValue::null_bulk_string().serialize();
}

std::string RESPFormatter::string(const std::string& s) {
    return RESPValue::bulk_string(s).serialize();
}

std::string RESPFormatter::integer(int64_t i) {
    return RESPValue::integer(i).serialize();
}

std::string RESPFormatter::array(const std::vector<std::string>& strings) {
    std::vector<RESPValue> values;
    for (const auto& s : strings) {
        values.push_back(RESPValue::bulk_string(s));
    }
    return RESPValue::array(values).serialize();
}

} // namespace kvstore
