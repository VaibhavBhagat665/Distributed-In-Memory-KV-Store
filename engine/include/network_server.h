#pragma once

#include "hash_table.h"
#include "resp.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace kvstore {

// Network backend type
enum class NetworkBackend {
    Epoll,
    IOUring
};

// Command handler function
using CommandHandler = std::function<std::string(const RESPCommand&)>;

// Abstract network server interface
class NetworkServer {
public:
    virtual ~NetworkServer() = default;
    
    // Start server on specified port
    virtual bool start(uint16_t port) = 0;
    
    // Stop server
    virtual void stop() = 0;
    
    // Run event loop (blocking)
    virtual void run() = 0;
    
    // Set command handler
    void set_command_handler(CommandHandler handler) {
        command_handler_ = std::move(handler);
    }
    
protected:
    CommandHandler command_handler_;
};

// Client connection state
struct ClientConnection {
    int fd;
    RESPParser parser;
    std::string write_buffer;
    bool should_close;
    
    ClientConnection(int socket_fd) 
        : fd(socket_fd), should_close(false) {}
};

} // namespace kvstore
