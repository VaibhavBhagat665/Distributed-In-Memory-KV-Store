#include "resp.h"
#include <cassert>
#include <iostream>

using namespace kvstore;

void test_simple_string() {
    std::cout << "Test: RESP Simple String..." << std::endl;
    
    RESPParser parser;
    std::string input = "+OK\r\n";
    
    auto result = parser.parse(input.data(), input.size());
    assert(result.has_value());
    assert(result->type == RESPType::SimpleString);
    assert(result->string_value == "OK");
    
    // Test serialization
    assert(result->serialize() == input);
    
    std::cout << "  PASSED" << std::endl;
}

void test_error() {
    std::cout << "Test: RESP Error..." << std::endl;
    
    RESPParser parser;
    std::string input = "-Error message\r\n";
    
    auto result = parser.parse(input.data(), input.size());
    assert(result.has_value());
    assert(result->type == RESPType::Error);
    assert(result->string_value == "Error message");
    
    assert(result->serialize() == input);
    
    std::cout << "  PASSED" << std::endl;
}

void test_integer() {
    std::cout << "Test: RESP Integer..." << std::endl;
    
    RESPParser parser;
    std::string input = ":1000\r\n";
    
    auto result = parser.parse(input.data(), input.size());
    assert(result.has_value());
    assert(result->type == RESPType::Integer);
    assert(result->int_value == 1000);
    
    assert(result->serialize() == input);
    
    std::cout << "  PASSED" << std::endl;
}

void test_bulk_string() {
    std::cout << "Test: RESP Bulk String..." << std::endl;
    
    RESPParser parser;
    std::string input = "$6\r\nfoobar\r\n";
    
    auto result = parser.parse(input.data(), input.size());
    assert(result.has_value());
    assert(result->type == RESPType::BulkString);
    assert(result->string_value == "foobar");
    
    assert(result->serialize() == input);
    
    std::cout << "  PASSED" << std::endl;
}

void test_null_bulk_string() {
    std::cout << "Test: RESP Null Bulk String..." << std::endl;
    
    RESPParser parser;
    std::string input = "$-1\r\n";
    
    auto result = parser.parse(input.data(), input.size());
    assert(result.has_value());
    assert(result->type == RESPType::NullBulkString);
    
    assert(result->serialize() == input);
    
    std::cout << "  PASSED" << std::endl;
}

void test_array() {
    std::cout << "Test: RESP Array..." << std::endl;
    
    RESPParser parser;
    std::string input = "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    
    auto result = parser.parse(input.data(), input.size());
    assert(result.has_value());
    assert(result->type == RESPType::Array);
    assert(result->array_value.size() == 2);
    assert(result->array_value[0].string_value == "foo");
    assert(result->array_value[1].string_value == "bar");
    
    assert(result->serialize() == input);
    
    std::cout << "  PASSED" << std::endl;
}

void test_command_parsing() {
    std::cout << "Test: RESP Command Parsing..." << std::endl;
    
    // Parse SET command
    RESPParser parser;
    std::string input = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
    
    auto result = parser.parse(input.data(), input.size());
    assert(result.has_value());
    
    auto cmd = RESPParser::to_command(*result);
    assert(cmd.has_value());
    assert(cmd->command == "SET");
    assert(cmd->args.size() == 2);
    assert(cmd->args[0] == "key");
    assert(cmd->args[1] == "value");
    
    std::cout << "  PASSED" << std::endl;
}

void test_pipelined_requests() {
    std::cout << "Test: RESP Pipelined Requests..." << std::endl;
    
    // Multiple commands in one buffer
    std::string input = "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n"
                       "*3\r\n$3\r\nSET\r\n$4\r\nkey2\r\n$6\r\nvalue2\r\n";
    
    RESPParser parser;
    
    // Parse first command
    auto result1 = parser.parse(input.data(), input.size());
    assert(result1.has_value());
    auto cmd1 = RESPParser::to_command(*result1);
    assert(cmd1.has_value());
    assert(cmd1->command == "GET");
    assert(cmd1->args[0] == "key");
    
    // Parse second command (parser continues from internal buffer)
    auto result2 = parser.parse("", 0);
    assert(result2.has_value());
    auto cmd2 = RESPParser::to_command(*result2);
    assert(cmd2.has_value());
    assert(cmd2->command == "SET");
    assert(cmd2->args[0] == "key2");
    assert(cmd2->args[1] == "value2");
    
    std::cout << "  PASSED" << std::endl;
}

void test_partial_input() {
    std::cout << "Test: RESP Partial Input..." << std::endl;
    
    RESPParser parser;
    
    // Feed partial bulk string
    std::string part1 = "$6\r\nfo";
    auto result1 = parser.parse(part1.data(), part1.size());
    assert(!result1.has_value());  // Incomplete
    
    // Feed remaining data
    std::string part2 = "obar\r\n";
    auto result2 = parser.parse(part2.data(), part2.size());
    assert(result2.has_value());
    assert(result2->type == RESPType::BulkString);
    assert(result2->string_value == "foobar");
    
    std::cout << "  PASSED" << std::endl;
}

void test_formatter() {
    std::cout << "Test: RESP Formatter..." << std::endl;
    
    assert(RESPFormatter::ok() == "+OK\r\n");
    assert(RESPFormatter::error("ERR") == "-ERR\r\n");
    assert(RESPFormatter::null() == "$-1\r\n");
    assert(RESPFormatter::string("hello") == "$5\r\nhello\r\n");
    assert(RESPFormatter::integer(42) == ":42\r\n");
    
    std::string arr = RESPFormatter::array({"foo", "bar"});
    assert(arr == "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    
    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "==================" << std::endl;
    std::cout << "RESP Protocol Tests" << std::endl;
    std::cout << "==================" << std::endl;
    
    test_simple_string();
    test_error();
    test_integer();
    test_bulk_string();
    test_null_bulk_string();
    test_array();
    test_command_parsing();
    test_pipelined_requests();
    test_partial_input();
    test_formatter();
    
    std::cout << "\nAll RESP tests passed! (10/10)" << std::endl;
    return 0;
}
